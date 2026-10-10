// SPDX-License-Identifier: MIT
// VM operations apply only to this process. Never change executable protections.
static NSString *RVDBGImageUUID(const struct mach_header *header) {
    if (!header || header->magic!=MH_MAGIC_64 || header->ncmds>4096 || header->sizeofcmds>1024*1024) return nil;
    const uint8_t *cursor=(const uint8_t *)header+sizeof(struct mach_header_64),*end=cursor+header->sizeofcmds;
    for (uint32_t index=0;index<header->ncmds;index++) {
        if ((size_t)(end-cursor)<sizeof(struct load_command)) return nil;const struct load_command *command=(const void *)cursor;
        if (command->cmdsize<sizeof(*command) || command->cmdsize>(size_t)(end-cursor)) return nil;
        if (command->cmd==LC_UUID && command->cmdsize>=sizeof(struct uuid_command)) {
            const struct uuid_command *uuid=(const void *)cursor;return [[NSUUID alloc] initWithUUIDBytes:uuid->uuid].UUIDString;
        }cursor+=command->cmdsize;
    }
    return nil;
}
static NSArray *RVDBGImages(void) {
    NSMutableArray *images=[NSMutableArray array];uint32_t count=MIN(_dyld_image_count(),1024u);
    for (uint32_t index=0;index<count;index++) {
        const struct mach_header *header=_dyld_get_image_header(index);const char *path=_dyld_get_image_name(index);
        NSString *uuid=RVDBGImageUUID(header);if (!uuid) continue;
        [images addObject:@{@"uuid":uuid,@"base":RVDBGHex((uint64_t)header),@"slide":RVDBGHex((uint64_t)_dyld_get_image_vmaddr_slide(index)),@"name":path ? [[NSString stringWithUTF8String:path] lastPathComponent] ?: @"image" : @"image"}];
    }
    return images;
}
static BOOL RVDBGRegion(uint64_t address,uint64_t *base,uint64_t *size,vm_prot_t *protection,kern_return_t *error) {
    vm_address_t cursor=address;vm_size_t length=0;natural_t depth=0;
    vm_region_submap_info_data_64_t info;mach_msg_type_number_t count=VM_REGION_SUBMAP_INFO_COUNT_64;
    for (NSUInteger pass=0;pass<32;pass++) {
        count=VM_REGION_SUBMAP_INFO_COUNT_64;*error=vm_region_recurse_64(mach_task_self(),&cursor,&length,&depth,(vm_region_recurse_info_t)&info,&count);
        if (*error!=KERN_SUCCESS) return NO;
        if (info.is_submap) { depth++;continue; }
        *base=cursor;*size=length;*protection=info.protection;return YES;
    }*error=KERN_RESOURCE_SHORTAGE;return NO;
}
static BOOL RVDBGRange(uint64_t address,NSUInteger length,BOOL writable,NSString **failure) {
    if (!address || !length || address>UINT64_MAX-length) { *failure=@"invalid_memory_range";return NO; }
    uint64_t cursor=address,end=address+length;
    for (NSUInteger index=0;cursor<end && index<64;index++) {
        uint64_t base=0,size=0;vm_prot_t protection=0;kern_return_t error=0;
        if (!RVDBGRegion(cursor,&base,&size,&protection,&error) || base>cursor || !size || base>UINT64_MAX-size || cursor>=base+size) { *failure=@"memory_region_unmapped";return NO; }
        if (!(protection&VM_PROT_READ) || (writable && (!(protection&VM_PROT_WRITE) || (protection&VM_PROT_EXECUTE)))) { *failure=writable ? @"region_not_writable_data" : @"region_not_readable";return NO; }
        cursor=MIN(end,base+size);
    }if (cursor<end) { *failure=@"memory_region_budget";return NO; }return YES;
}
static NSData *RVDBGReadBytes(uint64_t address,NSUInteger length,kern_return_t *error) {
    NSMutableData *bytes=[NSMutableData dataWithLength:length];vm_size_t actual=0;
    *error=vm_read_overwrite(mach_task_self(),address,length,(vm_address_t)bytes.mutableBytes,&actual);
    if (*error!=KERN_SUCCESS || actual!=length) return nil;return bytes;
}
static BOOL RVDBGRequestAddress(NSDictionary *args,uint64_t *address) {
    if (args[@"address"]) return !args[@"image_uuid"] && !args[@"offset"] && RVDBGAddress(args[@"address"],address);
    uint64_t offset=0;if (![args[@"image_uuid"] isKindOfClass:NSString.class] || !RVDBGAddress(args[@"offset"],&offset)) return NO;
    for (NSDictionary *image in RVDBGImages()) if ([image[@"uuid"] caseInsensitiveCompare:args[@"image_uuid"]]==NSOrderedSame) {
        uint64_t base=0;if (!RVDBGAddress(image[@"base"],&base) || base>UINT64_MAX-offset) return NO;*address=base+offset;return YES;
    }
    return NO;
}
static NSDictionary *RVDBGMemory(NSString *operation,NSDictionary *args,NSString *owner) {
    RVDBGState *state=RVDBGState.shared;
    if ([operation isEqual:@"debug.images"]) return @{@"ok":@YES,@"images":RVDBGImages()};
    if ([operation isEqual:@"debug.regions"]) {
        uint64_t cursor=0;if (args[@"cursor"] && !RVDBGAddress(args[@"cursor"],&cursor)) return RVDBGFail(@"invalid_region_cursor");
        NSMutableArray *regions=[NSMutableArray array];BOOL complete=NO;
        for (NSUInteger index=0;index<128;index++) {
            uint64_t base=0,size=0;vm_prot_t protection=0;kern_return_t error=0;
            if (!RVDBGRegion(cursor,&base,&size,&protection,&error)) { complete=YES;break; }
            if (!size || base>UINT64_MAX-size || base+size<=cursor) { complete=YES;break; }
            [regions addObject:@{@"address":RVDBGHex(base),@"size":RVDBGHex(size),@"readable":@((protection&VM_PROT_READ)!=0),@"writable":@((protection&VM_PROT_WRITE)!=0),@"executable":@((protection&VM_PROT_EXECUTE)!=0)}];cursor=base+size;
        }
        return @{@"ok":@YES,@"regions":regions,@"next_cursor":RVDBGHex(cursor),@"complete":@(complete)};
    }
    if ([operation isEqual:@"debug.allocate"]) {
        NSUInteger size=0;if (!RVDBGBound(args[@"length"],1,65536,&size)) return RVDBGFail(@"allocation_size_limit");
        @synchronized(state) {
            if (state.allocations.count>=16) return RVDBGFail(@"allocation_limit");vm_address_t address=0;
            kern_return_t error=vm_allocate(mach_task_self(),&address,size,VM_FLAGS_ANYWHERE);if (error!=KERN_SUCCESS) return @{@"ok":@NO,@"error":@"allocation_failed",@"kernel_status":@(error)};
            uint64_t allocation[2]={address,size};NSString *key=[owner stringByAppendingFormat:@"/%@",RVDBGHex(address)];state.allocations[key]=[NSData dataWithBytes:allocation length:sizeof(allocation)];
            return @{@"ok":@YES,@"address":RVDBGHex(address),@"length":@(size)};
        }
    }
    if ([operation isEqual:@"debug.free"]) {
        uint64_t address=0;if (!RVDBGAddress(args[@"address"],&address)) return RVDBGFail(@"invalid_address");
        @synchronized(state) {
            NSString *key=[owner stringByAppendingFormat:@"/%@",RVDBGHex(address)];NSData *allocation=state.allocations[key];if (!allocation) return RVDBGFail(@"allocation_not_owned");
            uint64_t saved[2];[allocation getBytes:saved length:sizeof(saved)];kern_return_t error=vm_deallocate(mach_task_self(),saved[0],saved[1]);
            if (error==KERN_SUCCESS) [state.allocations removeObjectForKey:key];return @{@"ok":@(error==KERN_SUCCESS),@"kernel_status":@(error)};
        }
    }
    uint64_t address=0;NSUInteger length=0;NSData *replacement=nil,*expected=nil;NSString *transaction=nil;
    BOOL rollback=[operation isEqual:@"debug.rollback"],write=[operation isEqual:@"debug.write"] || rollback;
    if (rollback) {
        if (![args[@"transaction"] isKindOfClass:NSString.class]) return RVDBGFail(@"invalid_transaction");transaction=args[@"transaction"];
        NSDictionary *record;@synchronized(state) { record=state.transactions[transaction]; }
        if (!record || ![record[@"owner"] isEqual:owner]) return RVDBGFail(@"transaction_not_owned");
        if (!RVDBGAddress(record[@"address"],&address)) return RVDBGFail(@"invalid_transaction_address");replacement=record[@"before"];expected=record[@"after"];length=replacement.length;
    }else {
        if (!RVDBGRequestAddress(args,&address)) return RVDBGFail(@"invalid_or_unresolved_address");
        if (write) {
            if (![args[@"bytes"] isKindOfClass:NSString.class] || ![args[@"expected"] isKindOfClass:NSString.class]) return RVDBGFail(@"expected_and_replacement_required");
            replacement=[[NSData alloc] initWithBase64EncodedString:args[@"bytes"] options:0];expected=[[NSData alloc] initWithBase64EncodedString:args[@"expected"] options:0];length=replacement.length;
            if (!length || length>1024 || length!=expected.length) return RVDBGFail(@"write_size_or_expected_mismatch");
        }else if (!RVDBGBound(args[@"length"],1,65536,&length)) return RVDBGFail(@"read_size_limit");
    }
    NSString *failure=nil;if (!RVDBGRange(address,length,write,&failure)) return RVDBGFail(failure);
    kern_return_t error=0;NSData *before=RVDBGReadBytes(address,length,&error);
    if (!before) return @{@"ok":@NO,@"error":@"memory_read_failed",@"kernel_status":@(error)};
    if (!write) return @{@"ok":@YES,@"address":RVDBGHex(address),@"length":@(before.length),@"bytes":[before base64EncodedStringWithOptions:0],@"kernel_status":@(error)};
    if (![before isEqual:expected]) return @{@"ok":@NO,@"error":@"expected_bytes_changed",@"current":[before base64EncodedStringWithOptions:0]};
    @synchronized(state) {
        if (!rollback && state.transactions.count>=16) return RVDBGFail(@"transaction_limit");
        error=vm_write(mach_task_self(),address,(vm_offset_t)replacement.bytes,(mach_msg_type_number_t)length);
        kern_return_t readError=0;NSData *after=RVDBGReadBytes(address,length,&readError);BOOL matched=after && [after isEqual:replacement];
        if (!rollback && (error==KERN_SUCCESS || ![before isEqual:after])) {
            transaction=NSUUID.UUID.UUIDString;state.transactions[transaction]=@{@"owner":owner,@"address":RVDBGHex(address),@"before":before,@"after":replacement};
        }else if (rollback && matched) [state.transactions removeObjectForKey:transaction];
        return @{@"ok":@(error==KERN_SUCCESS && matched),@"address":RVDBGHex(address),@"transaction":transaction ?: @"none",@"kernel_status":@(error),@"readback_status":@(readError),@"readback_matches":@(matched),@"atomic":@NO,@"before":[before base64EncodedStringWithOptions:0],@"after":after ? [after base64EncodedStringWithOptions:0] : @"unavailable"};
    }
}

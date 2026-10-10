// SPDX-License-Identifier: MIT
#include "RVRuntimeFields.h"
#include "RVRuntimeFieldCatalog.inc"
static NSDictionary *RVRuntimeFieldSpec(NSString *owner,NSString *name) {
    for (NSDictionary *spec in RVRuntimeFieldCatalog())
        if ([spec[@"owner"] isEqual:owner] && [spec[@"name"] isEqual:name]) return spec;
    return nil;
}
static Ivar RVRuntimeResolveField(id object,NSDictionary *spec,NSDictionary **metadata) {
    if (!NSThread.isMainThread || !object || !spec) return NULL;
    Class owner=NSClassFromString(spec[@"owner"]);
    if (!owner || ![object isKindOfClass:owner]) return NULL;
    // Require an ivar declared by this owner, not a same-named inherited field.
    unsigned int count=0;Ivar *fields=class_copyIvarList(owner,&count);Ivar field=NULL;
    for (unsigned int i=0;i<count;i++) if (strcmp(ivar_getName(fields[i]),[spec[@"name"] UTF8String])==0) { field=fields[i];break; }
    free(fields);if (!field) return NULL;
    const char *encoding=ivar_getTypeEncoding(field);
    if (!encoding || strcmp(encoding,[spec[@"encoding"] UTF8String]) || encoding[0]!='@') return NULL;
    ptrdiff_t offset=ivar_getOffset(field);Class parent=class_getSuperclass(owner);
    size_t inherited=parent ? class_getInstanceSize(parent) : 0,size=class_getInstanceSize(owner);
    if (!rv_runtime_field_bounds(offset,sizeof(id),inherited,size)) return NULL;
    if (metadata) *metadata=@{@"owner":spec[@"owner"],@"name":spec[@"name"],@"encoding":spec[@"encoding"],
        @"static_offset":spec[@"static_offset"],@"runtime_offset":@(offset),@"instance_size":@(size),
        @"superclass_size":@(inherited),@"object_read_api":@"object_getIvar",@"symbolic":@YES};
    return field;
}
static id RVRuntimeFieldObject(id object,NSString *owner,NSString *name,NSDictionary **metadata) {
    Ivar field=RVRuntimeResolveField(object,RVRuntimeFieldSpec(owner,name),metadata);
    return field ? object_getIvar(object,field) : nil;
}

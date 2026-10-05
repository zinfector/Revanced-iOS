// SPDX-License-Identifier: MIT
// User-initiated service actions; network work and proof-of-work stay off the UI thread.
typedef void (^RVServiceResult)(id,NSString *);
static BOOL RVVoteBusy;
static NSTimeInterval RVVoteRetryAfter;
static void RVNotice(UIViewController *presenter,NSString *title,NSString *message) {
    dispatch_async(dispatch_get_main_queue(),^{
        UIViewController *top=presenter;while (top.presentedViewController) top=top.presentedViewController;
        UIAlertController *alert=[UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleCancel handler:nil]];
        [top presentViewController:alert animated:YES completion:nil];
    });
}
static NSURL *RVServiceURL(NSString *base,NSDictionary *query) {
    NSURLComponents *url=[NSURLComponents componentsWithString:base];NSMutableArray *items=[NSMutableArray array];
    for (NSString *key in query) [items addObject:[NSURLQueryItem queryItemWithName:key value:[query[key] description]]];
    url.queryItems=items.count ? items : nil;return url.URL;
}
static void RVServiceRequest(NSURL *url,NSString *method,NSDictionary *body,RVServiceResult result) {
    NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:url];request.HTTPMethod=method;request.timeoutInterval=15;
    [request setValue:@"RVPort/0.3.0" forHTTPHeaderField:@"User-Agent"];
    if (body) { request.HTTPBody=[NSJSONSerialization dataWithJSONObject:body options:0 error:nil];[request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"]; }
    [[[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:^(NSData *data,NSURLResponse *response,NSError *error) {
        NSInteger status=[(NSHTTPURLResponse *)response statusCode];
        if (error || status<200 || status>=300 || data.length>1024*1024) {
            result(nil,error.localizedDescription ?: [NSString stringWithFormat:@"Service returned HTTP %ld",(long)status]);return;
        }
        id json=data.length ? [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingFragmentsAllowed error:nil] : @YES;
        result(json ?: @YES,nil);
    }] resume];
}
static NSString *RVLocalServiceID(NSString *key,NSUInteger length) {
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    NSString *value=[defaults stringForKey:key];
    if (value.length==length) return value;
    NSString *alphabet=@"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    NSMutableString *fresh=[NSMutableString string];
    for (NSUInteger i=0;i<length;i++) [fresh appendFormat:@"%C",[alphabet characterAtIndex:arc4random_uniform((uint32_t)alphabet.length)]];
    [defaults setObject:fresh forKey:key];return fresh;
}
static NSString *RVSolvePuzzle(id puzzle) {
    if (![puzzle isKindOfClass:NSDictionary.class] || ![puzzle[@"challenge"] isKindOfClass:NSString.class] || ![puzzle[@"difficulty"] isKindOfClass:NSNumber.class]) return nil;
    double raw=[puzzle[@"difficulty"] doubleValue];
    if (!isfinite(raw) || raw<0 || raw>22 || floor(raw)!=raw) return nil;
    unsigned int difficulty=(unsigned int)raw;
    NSData *challenge=[[NSData alloc] initWithBase64EncodedString:puzzle[@"challenge"] options:0];if (challenge.length!=16) return nil;
    uint8_t buffer[20]={0},digest[CC_SHA512_DIGEST_LENGTH];memcpy(buffer+4,challenge.bytes,16);
    NSTimeInterval deadline=NSDate.timeIntervalSinceReferenceDate+3;
    uint32_t limit=(1u<<(difficulty+1))*5u;
    for (uint32_t nonce=0;nonce<limit;nonce++) {
        if (!(nonce&4095) && NSDate.timeIntervalSinceReferenceDate>deadline) return nil;
        buffer[0]=nonce;buffer[1]=nonce>>8;buffer[2]=nonce>>16;buffer[3]=nonce>>24;
        CC_SHA512(buffer,sizeof(buffer),digest);unsigned int zeroes=0;
        for (NSUInteger i=0;i<sizeof(digest);i++) {
            if (!digest[i]) { zeroes+=8;continue; }
            for (uint8_t mask=128;mask && !(digest[i]&mask);mask>>=1) zeroes++;
            break;
        }
        if (zeroes>=difficulty) return [[NSData dataWithBytes:buffer length:4] base64EncodedStringWithOptions:0];
    }
    return nil;
}
static void RVRYDSendVote(NSString *user,NSString *video,NSNumber *value,RVServiceResult complete) {
    RVServiceRequest([NSURL URLWithString:@"https://returnyoutubedislikeapi.com/interact/vote"],@"POST",@{@"userId":user,@"videoId":video,@"value":value},^(id puzzle,NSString *error) {
        NSString *solution=error ? nil : RVSolvePuzzle(puzzle);
        if (!solution) { complete(nil,error ?: @"Vote puzzle could not be solved within the local work limit.");return; }
        RVServiceRequest([NSURL URLWithString:@"https://returnyoutubedislikeapi.com/interact/confirmVote"],@"POST",@{@"userId":user,@"videoId":video,@"solution":solution},complete);
    });
}
static void RVRYDVote(UIViewController *presenter,NSString *video,NSNumber *value) {
    if (RVVoteBusy || NSDate.timeIntervalSinceReferenceDate<RVVoteRetryAfter) { RVNotice(presenter,@"Return YouTube Dislike",@"Another vote is pending, or the service is cooling down. Try again later.");return; }
    if (!RVEnabled(@"ryd_voting") || ![video isEqual:RVObject(RVCurrentPlayback,"contentVideoID")]) return;
    RVVoteBusy=YES;NSString *user=RVLocalServiceID(@"RVPort.RYD.user",36);
    RVServiceResult complete=^(id result,NSString *error) {
        if (!error && [result isKindOfClass:NSNumber.class] && ![result boolValue]) error=@"The service did not confirm the vote.";
        dispatch_async(dispatch_get_main_queue(),^{ RVVoteBusy=NO;RVVoteRetryAfter=NSDate.timeIntervalSinceReferenceDate+(error ? 120 : 5); });
        RVNotice(presenter,@"Return YouTube Dislike",error ?: @"Service vote confirmed. This action does not change your YouTube account's vote.");
    };
    BOOL registered=[NSUserDefaults.standardUserDefaults boolForKey:@"RVPort.RYD.registered"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        if (registered) { RVRYDSendVote(user,video,value,complete);return; }
        NSURL *registration=RVServiceURL(@"https://returnyoutubedislikeapi.com/puzzle/registration",@{@"userId":user});
        RVServiceRequest(registration,@"GET",nil,^(id puzzle,NSString *error) {
            NSString *solution=error ? nil : RVSolvePuzzle(puzzle);
            if (!solution) { complete(nil,error ?: @"Registration puzzle could not be solved within the local work limit.");return; }
            RVServiceRequest(registration,@"POST",@{@"solution":solution},^(id result,NSString *registrationError) {
                if (!registrationError && [result isKindOfClass:NSNumber.class] && ![result boolValue]) registrationError=@"The service did not confirm registration.";
                if (registrationError) { complete(nil,registrationError);return; }
                [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"RVPort.RYD.registered"];
                RVRYDSendVote(user,video,value,complete);
            });
        });
    });
}
static BOOL RVSeekSeconds(double seconds) {
    if (!isfinite(seconds) || seconds<0 || !RVOrdinaryContent(RVCurrentPlayback)) return NO;
    Class times=NSClassFromString(@"YTSingleVideoTime");
    if (!RVCan(times,"timeWithTime:CPN:",@"@@:d@") || !RVCan(RVCurrentPlayback,"seekToTime:toleranceBefore:toleranceAfter:seekSource:",@"v@:@ddi")) return NO;
    NSString *cpn=RVObject(RVObject(RVCurrentPlayback,"contentVideoCurrentTime"),"CPN");
    id target=((id(*)(id,SEL,double,id))objc_msgSend)(times,sel_registerName("timeWithTime:CPN:"),seconds,cpn);if (!target) return NO;
    ((void(*)(id,SEL,id,double,double,int))objc_msgSend)(RVCurrentPlayback,sel_registerName("seekToTime:toleranceBefore:toleranceAfter:seekSource:"),target,0,0,0);return YES;
}
static void RVSBSubmit(UIViewController *presenter,NSString *video,NSString *category) {
    BOOL point=[category isEqual:@"poi_highlight"];
    UIAlertController *editor=[UIAlertController alertControllerWithTitle:@"Submit SponsorBlock segment" message:point ? @"Highlight time in seconds. Review the point before submitting." : [NSString stringWithFormat:@"%@ — start/end in seconds. Review the range before submitting.",category] preferredStyle:UIAlertControllerStyleAlert];
    double now=RVDouble(RVCurrentPlayer,"currentVideoMediaTime");
    [editor addTextFieldWithConfigurationHandler:^(UITextField *field) { field.placeholder=@"Start seconds";field.keyboardType=UIKeyboardTypeDecimalPad;field.text=isfinite(now) ? [NSString stringWithFormat:@"%.2f",now] : @""; }];
    if (!point) [editor addTextFieldWithConfigurationHandler:^(UITextField *field) { field.placeholder=@"End seconds";field.keyboardType=UIKeyboardTypeDecimalPad; }];
    [editor addAction:[UIAlertAction actionWithTitle:@"Submit reviewed segment" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        double values[2]={0};BOOL valid=YES;
        for (NSUInteger i=0;i<(point ? 1u : 2u);i++) { NSScanner *scanner=[NSScanner scannerWithString:editor.textFields[i].text ?: @""];valid=valid && [scanner scanDouble:&values[i]] && scanner.isAtEnd; }
        if (point) values[1]=values[0];
        double duration=RVDouble(RVCurrentPlayer,"currentVideoTotalMediaTime");
        if (!valid || !RVIntervalValid((RVInterval){values[0],values[1]},point,0) || !isfinite(duration) || duration<=0 || values[1]>duration || ![video isEqual:RVObject(RVCurrentPlayback,"contentVideoID")]) { RVNotice(presenter,@"SponsorBlock",@"Invalid range, unknown video duration, or the video has changed.");return; }
        NSDictionary *body=@{@"userID":RVLocalServiceID(@"RVPort.SB.user",36),@"videoID":video,@"videoDuration":@(duration),@"userAgent":@"RVPort/0.3.0",@"segments":@[@{@"segment":@[@(values[0]),@(values[1])],@"category":category,@"actionType":point ? @"poi" : @"skip"}]};
        RVServiceRequest([NSURL URLWithString:@"https://sponsor.ajay.app/api/skipSegments"],@"POST",body,^(id result,NSString *error) { RVNotice(presenter,@"SponsorBlock",error ?: @"Segment submitted for service review."); });
    }]];
    [editor addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:editor animated:YES completion:nil];
}
static void RVSBContribute(UIViewController *presenter,NSString *video) {
    UIAlertController *menu=[UIAlertController alertControllerWithTitle:@"SponsorBlock contributions" message:@"Choose a retrieved segment to vote, or manually enter a new segment." preferredStyle:UIAlertControllerStyleAlert];
    RVPlaybackSession *session=RVSession(RVCurrentPlayback);
    for (NSDictionary *segment in session.rawSegments) {
        NSString *uuid=segment[@"UUID"];if (!uuid.length || uuid.length>128) continue;
        NSString *label=[NSString stringWithFormat:@"%@ %.1f–%.1f",segment[@"category"],[segment[@"start"] doubleValue],[segment[@"end"] doubleValue]];
        [menu addAction:[UIAlertAction actionWithTitle:label style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            UIAlertController *votes=[UIAlertController alertControllerWithTitle:label message:nil preferredStyle:UIAlertControllerStyleAlert];
            for (NSNumber *type in @[@1,@0,@20]) [votes addAction:[UIAlertAction actionWithTitle:type.intValue==1 ? @"Upvote" : type.intValue==0 ? @"Downvote" : @"Undo service vote" style:UIAlertActionStyleDefault handler:^(UIAlertAction *vote) {
                NSURL *url=RVServiceURL(@"https://sponsor.ajay.app/api/voteOnSponsorTime",@{@"userID":RVLocalServiceID(@"RVPort.SB.user",36),@"UUID":uuid,@"videoID":video,@"type":type});
                RVServiceRequest(url,@"POST",nil,^(id result,NSString *error) { RVNotice(presenter,@"SponsorBlock",error ?: @"Service vote accepted."); });
            }]];
            [votes addAction:[UIAlertAction actionWithTitle:@"Vote to change category…" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
                UIAlertController *categories=[UIAlertController alertControllerWithTitle:@"Propose segment category" message:nil preferredStyle:UIAlertControllerStyleAlert];
                for (NSString *category in @[@"sponsor",@"selfpromo",@"interaction",@"intro",@"outro",@"preview",@"music_offtopic",@"filler",@"hook"]) [categories addAction:[UIAlertAction actionWithTitle:category style:UIAlertActionStyleDefault handler:^(UIAlertAction *choice) {
                    NSURL *url=RVServiceURL(@"https://sponsor.ajay.app/api/voteOnSponsorTime",@{@"userID":RVLocalServiceID(@"RVPort.SB.user",36),@"UUID":uuid,@"category":category});
                    RVServiceRequest(url,@"POST",nil,^(id result,NSString *error) { RVNotice(presenter,@"SponsorBlock",error ?: @"Category vote accepted by the service."); });
                }]];
                [categories addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:categories animated:YES completion:nil];
            }]];
            [votes addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:votes animated:YES completion:nil];
        }]];
        if (menu.actions.count>=20) break;
    }
    [menu addAction:[UIAlertAction actionWithTitle:@"Submit new segment…" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        UIAlertController *categories=[UIAlertController alertControllerWithTitle:@"Segment category" message:nil preferredStyle:UIAlertControllerStyleAlert];
        for (NSString *category in RVSetting(@"sponsor_categories")) [categories addAction:[UIAlertAction actionWithTitle:category style:UIAlertActionStyleDefault handler:^(UIAlertAction *choice) { RVSBSubmit(presenter,video,category); }]];
        [categories addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:categories animated:YES completion:nil];
    }]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:menu animated:YES completion:nil];
}
static void RVSBCategorySettings(UIViewController *presenter) {
    UIAlertController *categories=[UIAlertController alertControllerWithTitle:@"SponsorBlock category behavior" message:@"These controls apply to categories fetched by sponsor_categories. Reopen the video after changing that JSON list." preferredStyle:UIAlertControllerStyleAlert];
    for (NSString *category in RVSetting(@"sponsor_categories")) [categories addAction:[UIAlertAction actionWithTitle:[NSString stringWithFormat:@"%@: %@",category,RVSponsorBehavior(category)] style:UIAlertActionStyleDefault handler:^(UIAlertAction *choice) {
        UIAlertController *behaviors=[UIAlertController alertControllerWithTitle:category message:nil preferredStyle:UIAlertControllerStyleAlert];
        for (NSString *behavior in @[@"skip",@"skip-once",@"manual-skip",@"seekbar-only",@"ignore"]) [behaviors addAction:[UIAlertAction actionWithTitle:behavior style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            id current=RVSetting(@"sponsor_behaviors");NSMutableDictionary *values=[current isKindOfClass:NSDictionary.class] ? [current mutableCopy] : [NSMutableDictionary dictionary];
            values[category]=behavior;[NSUserDefaults.standardUserDefaults setObject:values forKey:@"RVPort.sponsor_behaviors"];RVSegmentsChanged();
        }]];
        [behaviors addAction:[UIAlertAction actionWithTitle:@"Set marker color…" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            UIAlertController *color=[UIAlertController alertControllerWithTitle:@"Marker color" message:@"Enter #RRGGBB. Empty restores the category's default color." preferredStyle:UIAlertControllerStyleAlert];
            [color addTextFieldWithConfigurationHandler:^(UITextField *field) { field.placeholder=@"#00D400";field.autocapitalizationType=UITextAutocapitalizationTypeAllCharacters; }];
            [color addAction:[UIAlertAction actionWithTitle:@"Save" style:UIAlertActionStyleDefault handler:^(UIAlertAction *save) {
                NSString *value=color.textFields.firstObject.text ?: @"";
                if (value.length && [value rangeOfString:@"^#[0-9A-Fa-f]{6}$" options:NSRegularExpressionSearch].location==NSNotFound) { RVNotice(presenter,@"SponsorBlock",@"Expected #RRGGBB.");return; }
                id current=RVSetting(@"sponsor_colors");NSMutableDictionary *values=[current isKindOfClass:NSDictionary.class] ? [current mutableCopy] : [NSMutableDictionary dictionary];
                if (value.length) values[category]=value;else [values removeObjectForKey:category];
                [NSUserDefaults.standardUserDefaults setObject:values forKey:@"RVPort.sponsor_colors"];RVSegmentsChanged();
            }]];
            [color addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:color animated:YES completion:nil];
        }]];
        [behaviors addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:behaviors animated:YES completion:nil];
    }]];
    [categories addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:categories animated:YES completion:nil];
}
static void RVSBAccount(UIViewController *presenter) {
    NSString *user=RVLocalServiceID(@"RVPort.SB.user",36);
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    NSString *local=[NSString stringWithFormat:@"Local skip requests: %ld\nEstimated requested skip time: %.1f seconds\nThese counters record seek requests, not confirmed playback time saved.",(long)[defaults integerForKey:@"RVPort.SB.skipRequests"],[defaults doubleForKey:@"RVPort.SB.requestedSkipSeconds"]];
    UIAlertController *menu=[UIAlertController alertControllerWithTitle:@"SponsorBlock account and statistics" message:local preferredStyle:UIAlertControllerStyleAlert];
    [menu addAction:[UIAlertAction actionWithTitle:@"Fetch service statistics / VIP status" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSURL *url=RVServiceURL(@"https://sponsor.ajay.app/api/userInfo",@{@"userID":user});
        RVServiceRequest(url,@"GET",nil,^(id result,NSString *error) {
            if (error || ![result isKindOfClass:NSDictionary.class]) { RVNotice(presenter,@"SponsorBlock",error ?: @"Unexpected service response.");return; }
            NSMutableArray *lines=[NSMutableArray array];
            for (NSString *key in @[@"userName",@"reputation",@"segmentCount",@"ignoredSegmentCount",@"viewCount",@"minutesSaved",@"vip"]) {
                id value=result[key];if (![value isKindOfClass:NSString.class] && ![value isKindOfClass:NSNumber.class]) continue;
                NSString *line=[NSString stringWithFormat:@"%@: %@",key,value];[lines addObject:[line substringToIndex:MIN(line.length,256)]];
            }
            RVNotice(presenter,@"SponsorBlock service statistics",lines.count ? [lines componentsJoinedByString:@"\n"] : @"No supported statistics were returned.");
        });
    }]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Set public service username…" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        UIAlertController *editor=[UIAlertController alertControllerWithTitle:@"SponsorBlock public username" message:@"This changes the public name associated with this app's SponsorBlock service identity." preferredStyle:UIAlertControllerStyleAlert];
        [editor addTextFieldWithConfigurationHandler:^(UITextField *field) { field.placeholder=@"Public username";field.autocapitalizationType=UITextAutocapitalizationTypeNone; }];
        [editor addAction:[UIAlertAction actionWithTitle:@"Set public username" style:UIAlertActionStyleDefault handler:^(UIAlertAction *save) {
            NSString *name=editor.textFields.firstObject.text ?: @"";
            if (!name.length || name.length>32 || [name rangeOfCharacterFromSet:NSCharacterSet.controlCharacterSet].location!=NSNotFound) { RVNotice(presenter,@"SponsorBlock",@"Use 1–32 characters without control characters. The service validates the name.");return; }
            NSURL *url=RVServiceURL(@"https://sponsor.ajay.app/api/setUsername",@{@"userID":user,@"username":name});
            RVServiceRequest(url,@"POST",nil,^(id result,NSString *error) { RVNotice(presenter,@"SponsorBlock",error ?: @"Public username updated by the service."); });
        }]];
        [editor addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:editor animated:YES completion:nil];
    }]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:menu animated:YES completion:nil];
}
static void RVReadAnnouncements(UIViewController *presenter) {
    RVServiceRequest([NSURL URLWithString:@"https://api.revanced.app/v5/announcements/latest"],@"GET",nil,^(id json,NSString *error) {
        NSDictionary *announcement=nil;
        if ([json isKindOfClass:NSArray.class]) for (id item in json) {
            if ([item isKindOfClass:NSDictionary.class] && [item[@"tag"] isEqual:@"🎞️ YouTube"] && [item[@"announcement"] isKindOfClass:NSDictionary.class]) { announcement=item[@"announcement"];break; }
        }
        NSString *title=[announcement[@"title"] isKindOfClass:NSString.class] ? announcement[@"title"] : @"ReVanced announcements";
        NSString *content=[announcement[@"content"] isKindOfClass:NSString.class] ? announcement[@"content"] : @"No YouTube announcement was returned.";
        // Plain text only; no remote web view or script execution.
        content=[content stringByReplacingOccurrencesOfString:@"<[^>]*>" withString:@"" options:NSRegularExpressionSearch range:NSMakeRange(0,content.length)];
        RVNotice(presenter,title,error ?: [content substringToIndex:MIN(content.length,6000)]);
    });
}
static void RVAddServiceTools(UIAlertController *menu,UIViewController *presenter) {
    NSString *video=RVObject(RVCurrentPlayback,"contentVideoID");
    if (RVEnabled(@"ryd_voting") && RVVideoIDValid(video)) for (NSNumber *value in @[@1,@(-1),@0]) [menu addAction:[UIAlertAction actionWithTitle:value.intValue==1 ? @"RYD: submit like" : value.intValue==-1 ? @"RYD: submit dislike" : @"RYD: withdraw vote" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) { RVRYDVote(presenter,video,value); }]];
    if (RVEnabled(@"sponsorblock") && RVOrdinaryContent(RVCurrentPlayback)) {
        RVPlaybackSession *session=RVSession(RVCurrentPlayback);NSUInteger generation=session.generation;
        [menu addAction:[UIAlertAction actionWithTitle:@"Skip current SponsorBlock segment" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            if ([video isEqual:RVObject(RVCurrentPlayback,"contentVideoID")]) RVSPTool(@"manual",generation);
        }]];
        if (session.promptState.undo && session.promptState.undoRemaining>0) [menu addAction:[UIAlertAction actionWithTitle:@"Undo last SponsorBlock skip" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            if ([video isEqual:RVObject(RVCurrentPlayback,"contentVideoID")]) RVSPTool(@"undo",generation);
        }]];
        for (NSDictionary *segment in session.rawSegments) if ([segment[@"action"] isEqual:@"poi"] && ![@[@"ignore",@"seekbar-only"] containsObject:RVSponsorBehavior(segment[@"category"])]) {
            [menu addAction:[UIAlertAction actionWithTitle:@"Jump to SponsorBlock highlight" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
                if ([video isEqual:RVObject(RVCurrentPlayback,"contentVideoID")]) RVSPTool(@"highlight",generation);
            }]];break;
        }
    }
    if (RVEnabled(@"sponsorblock")) [menu addAction:[UIAlertAction actionWithTitle:@"SponsorBlock category settings…" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) { RVSBCategorySettings(presenter); }]];
    if (RVEnabled(@"sponsorblock_contribute") && RVVideoIDValid(video)) [menu addAction:[UIAlertAction actionWithTitle:@"SponsorBlock: vote or submit…" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) { RVSBContribute(presenter,video); }]];
    if (RVEnabled(@"sponsorblock_contribute")) [menu addAction:[UIAlertAction actionWithTitle:@"SponsorBlock account / statistics…" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) { RVSBAccount(presenter); }]];
    if (RVEnabled(@"announcements")) [menu addAction:[UIAlertAction actionWithTitle:@"Read ReVanced announcement" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) { RVReadAnnouncements(presenter); }]];
    if (RVEnabled(@"watch_history_dns")) [menu addAction:[UIAlertAction actionWithTitle:@"Check watch-history DNS" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
            struct addrinfo *addresses=NULL;int status=getaddrinfo("s.youtube.com",NULL,NULL,&addresses);
            if (addresses) freeaddrinfo(addresses);
            RVNotice(presenter,@"Watch-history DNS",status ? @"s.youtube.com could not be resolved. Check your DNS or ad-blocker configuration." : @"s.youtube.com resolved. This does not verify watch-history upload or account synchronization.");
        });
    }]];
}

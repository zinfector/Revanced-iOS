// Exercise production runtime preflight against this harness's loaded classes.
static void FixtureAdaptivePreflight(void) {
    NSMutableArray *contracts=[NSMutableArray array];
    for (size_t i=0;i<sizeof(RVAdaptiveContractIDs)/sizeof(RVAdaptiveContractIDs[0]);i++) {
        NSString *key=RVAdaptiveContractIDs[i];NSArray *parts=[key componentsSeparatedByString:@"|"];
        [contracts addObject:@{@"id":key,@"class_name":parts[0],@"kind":parts[1],@"selector":parts[2],
            @"abi":parts[3],@"resolution":@{@"status":@"runtime_framework"}}];
    }
    RVAdaptiveProfile=@{@"contracts":contracts,@"disabled_features":@[]};
    assert(RVAdaptivePreflight());
}

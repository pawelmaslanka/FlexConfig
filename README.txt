Design

┌──────────────────┐                  ┌──────────────────┐                ┌────────────────────────┐
│                  │                  │                  │                │                        │
│                  │  JSON over HTTP  │                  │      RPC       │                        │
│    FlexConfig    ├──────────────────┤    Middleware    ├────────────────┤   SDK/SAI/ASIC/DPDK    │
│                  │                  │                  │                │                        │
│                  │                  │                  │                │                        │
└────────┬─────────┘                  └──────────────────┘                └────────────────────────┘
         │
         │
         │ JSON                        Synchronizes state                  Program ASIC via SDK API,
         │ over                        beetween modules,                   SAI API or DPDK. It does
         │ HTTPS                       i.e. update VLAN                    not store any state
         │                             for ports in bundle
         │
   ┌─────┴──────┐                      Additionally it
   │            │                      synchronize the state
   │            │                      from other components
   │    User    │                      like nexthop checker,
   │            │                      RIB table (i.e. BGP
   │            │                      speaker), etc.
   └────────────┘
                                       This is also middleware
                                       to handle type of
                                       cables/connector
                                       (need FEC, enable AN,
                          ┼            set preemphasis settings
                                       in PHY, etc.)
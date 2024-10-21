Design & Idea

┌──────────────────┐                  ┌──────────────────┐                ┌────────────────────────┐
│                  │                  │                  │                │                        │
│                  │  JSON over HTTP  │                  │      RPC       │                        │
│    FlexConfig    ├──────────────────┤    Middleware    ├────────────────┤   SDK/SAI/ASIC/DPDK    │
│                  │                  │                  │                │ (Dataplane management) │
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

                                       Speaks with userspace
                                       daemons, like LACP,
                                       BGP


Rules during write schema:
* **@** - this is reference token. Reference to some **node**. It is evaluated as its corresponding node in other xpath. It is used at the end of expression. 
* **[@item]** - this is reference to node in container. It is evaluated as node in currently processing xpath.

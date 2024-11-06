What is FlexConfig?
The **Flexconfig** project is part of a larger project called **yaNOS** (yet another Networking Operating System).
The **FlexConfig** is an independent configuration manager that means it can be easily integrated with other system by adjustment of schema written in JSON format.
It works as a frontend of configuration component in the system so you have to delivery a backend server which receives a request(s) from **FlexConfig** and performs proper action in this system.
As a reference of place of the **FlexConfig** in a system, please look at section "Design & Ideas".
Here are main key points of the **FlexConfig** project:
* it is fully transactional mechanism - all operations are completed successfully or none
* supports such operations like diff, commit, rollback and commit-confirmed (automatically withdrawn changes after timeout) on the configuration file
* a syntax, format and validation of configuration file is mainly based on the JSON schema specification

Design & Idea (of yaNOS architecture)

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

Exceptions to JSON schema
* An **array** type is not supported. An aggregated values are implemented as a set of object with **null** type and value (a leaf)
* Additional properties are supoprted:
** action
** action-parameters
** delete-constraints
** delete-dependencies
** update-constraints
** update-dependencies

Rules during write schema:
* **@** - this is reference token. Reference to some **node**. It is evaluated as its corresponding node in other xpath. It is used at the end of expression. 
* **[@key]** - this is reference to node in container. It is evaluated as node in currently processing xpath.

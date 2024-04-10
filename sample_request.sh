echo "Startup config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

SESSION_TOKEN="HelloWorld!"
echo "Create new session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/session/token \
   -H 'Content-Type: application/text' \
   -d ${SESSION_TOKEN}`

if [ ${HTTP_STATUS} -eq 201 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get diff of valid config - add new interface 'eth1-2' and add it to vlan 2"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-10": {
        "members": {
          "eth1-10": null
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-2": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {}
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null
        },
        "untagged": {
          "eth1-2": null
        }
      }
    }
  }
}' | jq

echo "Post update good config without session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2",
        "value": {
            "speed": "100G"
        }
    },
    {
        "op": "add",
        "path": "/platform/port/eth1-2",
        "value": {
            "breakout-mode": "none"
        }
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "eth1-2": null
        }
    }
]'`

if [ ${HTTP_STATUS} -eq 499 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update good config with bad session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer INVALID_TOKEN" \
   -d '[
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2",
        "value": {
            "speed": "100G"
        }
    },
    {
        "op": "add",
        "path": "/platform/port/eth1-2",
        "value": {
            "breakout-mode": "none"
        }
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "eth1-2": null
        }
    }
]'`

if [ ${HTTP_STATUS} -eq 498 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config due to duplicated key at xpath '/interface/ethernet/eth1-1'"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-1",
        "value": {
            "speed": "400G"
        }
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2",
        "value": {
            "speed": "100G"
        }
    },
    {
        "op": "add",
        "path": "/platform/port/eth1-2",
        "value": {
            "breakout-mode": "none"
        }
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "eth1-2": null
        }
    }
]'`

if [ ${HTTP_STATUS} -eq 500 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update good config [1]"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2",
        "value": {
            "speed": "100G"
        }
    },
    {
        "op": "add",
        "path": "/platform/port/eth1-2",
        "value": {
            "breakout-mode": "none"
        }
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "eth1-2": null
        }
    }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

# echo "Wait till session token will expire"
# sleep 210

# echo "Apply candidate config should fail due to expired token"
# HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
#    -H 'Content-Type: application/json' \
#    -H "Authorization: Bearer ${SESSION_TOKEN}" \
#    -d ''`

# if [ ${HTTP_STATUS} -eq 498 ] 
# then 
#     echo "Successfully processed the request" 
# else 
#     echo "Failed to process the request (${HTTP_STATUS})"
#     exit 1
# fi

# echo "Post update config again after the time for the changes expired"
# HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
#    -H 'Content-Type: application/json' \
#    -H "Authorization: Bearer ${SESSION_TOKEN}" \
#    -d '[
#     {
#         "op": "add",
#         "path": "/interface/ethernet/eth1-2",
#         "value": {
#             "speed": "100G"
#         }
#     },
#     {
#         "op": "add",
#         "path": "/platform/port/eth1-2",
#         "value": {
#             "breakout-mode": "none"
#         }
#     },
#     {
#         "op": "add",
#         "path": "/vlan/2/members/untagged",
#         "value": {
#             "eth1-2": null
#         }
#     }
# ]'`

# if [ ${HTTP_STATUS} -eq 200 ] 
# then 
#     echo "Successfully processed the request" 
# else 
#     echo "Failed to process the request (${HTTP_STATUS})"
#     exit 1
# fi

echo "Apply candidate config"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no removed /interface/ethernet/eth1-2)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/lag-1",
        "value": {
            "members": {
                "eth1-2_1": null
            }
        }
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2_1",
        "value": {
        "speed": "fixed"
        }
    },
    {
        "op": "replace",
        "path": "/platform/port/eth1-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/2/members/untagged/eth1-2"
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "lag-1": null
        }
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no added /interface/ethernet/eth1-2_1)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/lag-1",
        "value": {
            "members": {
                "eth1-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth1-2"
    },
    {
        "op": "replace",
        "path": "/platform/port/eth1-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/2/members/untagged/eth1-2"
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "lag-1": null
        }
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no updated /platform/port/eth1-2/breakout-mode)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/lag-1",
        "value": {
            "members": {
                "eth1-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth1-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2_1",
        "value": {
        "speed": "fixed"
        }
    },
    {
        "op": "remove",
        "path": "/vlan/2/members/untagged/eth1-2"
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "lag-1": null
        }
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no removed /vlan/id/2/members/eth1-2)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/lag-1",
        "value": {
            "members": {
                "eth1-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth1-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2_1",
        "value": {
        "speed": "fixed"
        }
    },
    {
        "op": "replace",
        "path": "/platform/port/eth1-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "lag-1": null
        }
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no created /interface/aggregate-ethernet/lag-1)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "remove",
        "path": "/interface/ethernet/eth1-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2_1",
        "value": {
        "speed": "fixed"
        }
    },
    {
        "op": "replace",
        "path": "/platform/port/eth1-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/2/members/untagged/eth1-2"
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged",
        "value": {
            "lag-1": null
        }
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get candidate config"
curl -s -X GET http://localhost:8001/config/candidate \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -H 'Content-Type: application/json' | jq

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Get diff after breakout the port eth1-2"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-1": {
        "members": {
          "eth1-2_1": null
        }
      },
      "lag-10": {
        "members": {
          "eth1-10": null
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      },
      "eth1-2_1": {
        "speed": "fixed"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "4x100G"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {}
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null
        },
        "untagged": {
          "lag-1": null
        }
      }
    }
  }
}' | jq

echo "Post update good config [2]"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/interface/aggregate-ethernet/lag-1",
    "value": {
      "members": {
        "eth1-2_1": null
      }
    }
  },
  {
    "op": "remove",
    "path": "/interface/ethernet/eth1-2"
  },
  {
    "op": "add",
    "path": "/interface/ethernet/eth1-2_1",
    "value": {
      "speed": "fixed"
    }
  },
  {
    "op": "replace",
    "path": "/platform/port/eth1-2/breakout-mode",
    "value": "4x100G"
  },
  {
    "op": "remove",
    "path": "/vlan/2/members/untagged/eth1-2"
  },
  {
    "op": "add",
    "path": "/vlan/2/members/untagged/lag-1",
    "value": null
  }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Rollback the changes"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update good config [3]"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/lag-1",
        "value": {
            "members": {
                "eth1-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth1-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-2_1",
        "value": {
            "speed": "fixed"
        }
    },
    {
        "op": "replace",
        "path": "/platform/port/eth1-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/2/members/untagged/eth1-2"
    },
    {
        "op": "add",
        "path": "/vlan/2/members/untagged/lag-1",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Do not allow the changes to apply if the session token is different from the active one (the user who made the changes)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer NotActiveToken" \
   -d ''`

if [ ${HTTP_STATUS} -eq 498 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Apply the changes"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Try to delete invalid session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer InvalidToken" \
   -d ''`

if [ ${HTTP_STATUS} -eq 498 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Delete session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/session/token \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

SESSION_TOKEN="HelloWorld2!"
echo "Create new session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/session/token \
   -H 'Content-Type: application/text' \
   -d ${SESSION_TOKEN}`

if [ ${HTTP_STATUS} -eq 201 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Apply with success more complex diff config with two attributes embedded into xpath '/interface/ethernet/eth1-11'"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/lag-11",
        "value": {
            "members": {
                "eth1-11": null
            }
        }
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth1-11",
        "value": {
            "auto-negotiation": "enabled",
            "speed": "400G"
        }
    },
    {
        "op": "add",
        "path": "/platform/port/eth1-11",
        "value": {
            "breakout-mode": "none"
        }
    },
    {
        "op": "add",
        "path": "/protocol/lacp/lag-11",
        "value": {}
    },
    {
        "op": "add",
        "path": "/vlan/11",
        "value": {
            "members": {
                "native": {
                    "lag-11": null
                }
            }
        }
    }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Rollback the changes"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Get diff from invalid config due to membership of interface in LAG members when adding it to VLAN"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-1": {
        "members": {
          "eth1-2_1": null
        }
      },
      "lag-10": {
        "members": {
          "eth1-10": null
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      },
      "eth1-2_1": {
        "speed": "fixed"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "4x100G"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {}
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null,
          "eth1-10": null
        },
        "untagged": {
          "lag-1": null
        }
      }
    }
  }
}' | jq

echo "Apply invalid VLAN membership when interface is part of LAG members"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/vlan/2/members/tagged/eth1-10",
    "value": null
  }
]'`

if [ ${HTTP_STATUS} -eq 500 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get diff from invalid config due to membership of interface in VLAN members when adding it to LAG"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-1": {
        "members": {
          "eth1-2_1": null
        }
      },
      "lag-10": {
        "members": {
          "eth1-1": null,
          "eth1-10": null
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      },
      "eth1-2_1": {
        "speed": "fixed"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "4x100G"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {}
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null
        },
        "untagged": {
          "lag-1": null
        }
      }
    }
  }
}' | jq

echo "Apply invalid LAG membership when interface is part of VLAN members"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/interface/aggregate-ethernet/lag-10/members/eth1-1",
    "value": null
  }
]'`

if [ ${HTTP_STATUS} -eq 500 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get diff from invalid config due to non exists LAG members in LACP members configuration"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-1": {
        "members": {
          "eth1-2_1": null
        }
      },
      "lag-10": {
        "members": {
          "eth1-10": null
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      },
      "eth1-2_1": {
        "speed": "fixed"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "4x100G"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {
        "members": {
          "eth1-20": {}
        }
      }
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null
        },
        "untagged": {
          "lag-1": null
        }
      }
    }
  }
}' | jq

echo "Apply invalid LACP membership when member is not part of LAG"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/protocol/lacp/lag-10/members",
    "value": {
      "eth1-2_1": {}
    }
  }
]'`

if [ ${HTTP_STATUS} -eq 500 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Apply valid LACP membership config"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/protocol/lacp/lag-10/members",
    "value": {
      "eth1-10": {}
    }
  }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get candidate config"
curl -s -X GET http://localhost:8001/config/candidate \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -H 'Content-Type: application/json' | jq

echo "Apply candidate config"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Get diff from invalid config due to still referenced LAG members from LACP membership"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-1": {
        "members": {
          "eth1-2_1": null
        }
      },
      "lag-10": {
        "members": {
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      },
      "eth1-2_1": {
        "speed": "fixed"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "4x100G"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {
        "members": {
          "eth1-10": {}
        }
      }
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null
        },
        "untagged": {
          "lag-1": null
        }
      }
    }
  }
}' | jq

echo "Apply invalid LAG member removing when it still is reffering to LACP membership"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "remove",
    "path": "/interface/aggregate-ethernet/lag-10/members/eth1-10"
  }
]'`

if [ ${HTTP_STATUS} -eq 500 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Get diff from valid config where LACP member referring to LAG member"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-1": {
        "members": {
          "eth1-2_1": null
        }
      },
      "lag-10": {
        "members": {
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      },
      "eth1-2_1": {
        "speed": "fixed"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "4x100G"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {
        "members": {
        }
      }
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null
        },
        "untagged": {
          "lag-1": null
        }
      }
    }
  }
}' | jq

echo "Apply valid LAG member removing when it still is reffering to LACP membership"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "remove",
    "path": "/interface/aggregate-ethernet/lag-10/members/eth1-10"
  },
  {
    "op": "remove",
    "path": "/protocol/lacp/lag-10/members/eth1-10"
  }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Get diff from invalid config due to not exists member of RSTP bridge"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-1": {
        "members": {
          "eth1-2_1": null
        }
      },
      "lag-10": {
        "members": {
          "eth1-10": null
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      },
      "eth1-2_1": {
        "speed": "fixed"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "4x100G"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {
        "members": {
          "eth1-10": {}
        }
      }
    },
    "rstp": {
      "br-1": {
        "members": {
          "eth1-30": {}
        }
      }
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null
        },
        "untagged": {
          "lag-1": null
        }
      }
    }
  }
}' | jq

echo "Apply invalid config due to not exists member of RSTP bridge"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/protocol/rstp",
    "value": {
      "br-1": {
        "members": {
          "eth1-30": {}
        }
      }
    }
  }
]'`

if [ ${HTTP_STATUS} -eq 500 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Apply invalid config due to interface is part of LAG membership"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/protocol/rstp",
    "value": {
      "br-1": {
        "members": {
          "eth1-10": {}
        }
      }
    }
  }
]'`

if [ ${HTTP_STATUS} -eq 500 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Apply valid config RSTP bridge"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/protocol/rstp",
    "value": {
      "br-1": {
        "members": {
          "eth1-1": {}
        }
      }
    }
  }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Get candidate config"
curl -s -X GET http://localhost:8001/config/candidate \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -H 'Content-Type: application/json' | jq

echo "Apply candidate config"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get candidate config"
curl -s -X GET http://localhost:8001/config/candidate \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -H 'Content-Type: application/json' | jq

echo "Apply invalid LLDP config due to missing interface as a member"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/protocol/lldp",
    "value": {
      "enabled": "yes",
      "members": {
        "eth1-22": {}
      }
    }
  }
]'`

if [ ${HTTP_STATUS} -eq 500 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get diff after add LLDP config"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "interface": {
    "aggregate-ethernet": {
      "lag-1": {
        "members": {
          "eth1-2_1": null
        }
      },
      "lag-10": {
        "members": {
          "eth1-10": null
        }
      }
    },
    "ethernet": {
      "eth1-1": {
        "speed": "100G"
      },
      "eth1-10": {
        "speed": "100G"
      },
      "eth1-2_1": {
        "speed": "fixed"
      }
    }
  },
  "platform": {
    "port": {
      "eth1-1": {
        "breakout-mode": "none"
      },
      "eth1-10": {
        "breakout-mode": "none"
      },
      "eth1-2": {
        "breakout-mode": "4x100G"
      }
    }
  },
  "protocol": {
    "lacp": {
      "lag-10": {
        "members": {
          "eth1-10": {}
        }
      }
    },
    "lldp": {
      "enabled": "yes",
      "members": {
        "eth1-10": {}
      }
    },
    "rstp": {
      "br-1": {
        "members": {
          "eth1-1": {}
        }
      }
    }
  },
  "vlan": {
    "2": {
      "members": {
        "tagged": {
          "eth1-1": null
        },
        "untagged": {
          "lag-1": null
        }
      }
    }
  }
}' | jq

echo "Apply valid LLDP config"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
  {
    "op": "add",
    "path": "/protocol/lldp",
    "value": {
      "enabled": "yes",
      "members": {
        "eth1-10": {}
      }
    }
  }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get candidate config"
curl -s -X GET http://localhost:8001/config/candidate \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -H 'Content-Type: application/json' | jq

echo "Delete session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/session/token \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Successfully passed the test!"

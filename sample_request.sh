echo "Get running config"

curl -s -X GET http://localhost:8001/config/running/get \
   -H 'Content-Type: application/json' | jq

echo
echo "Update config about new gigabit-ethernet interface"

# curl -s -X POST http://localhost:8001/config/running/update \
#    -H 'Content-Type: application/json' \
#    -d '{
#             "platform": {
#                "port": {
#                   "ge-5": {
#                      "breakout-mode": "none"
#                   }
#                }
#             },
#             "interface": {
#                "gigabit-ethernet": {
#                   "ge-5": {
#                      "speed": "100G"
#                   }
#                }
#             }
#       }'

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running/get \
   -H 'Content-Type: application/json' | jq

echo

echo "Get diff config"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '{
    "interface": {
        "gigabit-ethernet": {
            "ge-1": {
                "speed": "100G"
            },
            "ge-2": {
                "speed": "100G"
            }
        },
        "aggregate-ethernet": {
            "ae-1": {
                "members": [
                    "ge-2"
                ]
            }
        }
    },
    "vlan": {
        "id": {
            "2": {
                "members": [
                    "ge-1"
                ]
            }
        }
    },
    "protocol": {
    },
    "platform": {
        "port": {
            "ge-1": {
                "breakout-mode": "none"
            },
            "ge-2": {
                "breakout-mode": "none"
            }
        }
    }
}' | jq

echo "Patch running config"
curl -s -X POST http://localhost:8001/config/update \
   -H 'Content-Type: application/json' \
   -d '[
  {
    "op": "add",
    "path": "/interface/aggregate-ethernet/ae-1",
    "value": {
      "members": [
        "ge-2"
      ]
    }
  },
  {
    "op": "add",
    "path": "/interface/gigabit-ethernet/ge-2",
    "value": {
      "speed": "100G"
    }
  }
]'

echo "Get candidate config"
curl -s -X GET http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' | jq

echo "Apply candidate config"
curl -s -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' -d ''

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running/get \
   -H 'Content-Type: application/json' | jq
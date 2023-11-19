curl -s -X POST http://localhost:8001/config/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/gigabit-ethernet/ge-2",
        "value": {
            "speed": "100G"
        }
    },
    {
        "op": "add",
        "path": "/platform/port/ge-2",
        "value": {
            "breakout-mode": "none"
        }
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ge-2",
        "value": null
    }
]'

curl -s -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' -d ''

curl -s -X POST http://localhost:8001/config/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/ae-1",
        "value": {
            "members": {
                "ge-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/gigabit-ethernet/ge-2"
    },
    {
        "op": "add",
        "path": "/interface/gigabit-ethernet/ge-2_1",
        "value": {}
    },
    {
        "op": "replace",
        "path": "/platform/port/ge-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/id/2/members/ge-2"
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ae-1",
        "value": null
    }
]'



exit


echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo
echo "Get diff config"
curl -s -X POST http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '{
    "interface": {
        "aggregate-ethernet": {},
        "gigabit-ethernet": {
            "ge-1": {
                "speed": "100G"
            },
            "ge-4": {
                "speed": "100G"
            }
        }
    },
    "platform": {
        "port": {
            "ge-1": {
                "breakout-mode": "none"
            },
            "ge-2": {
                "breakout-mode": "none"
            },
            "ge-4": {
                "breakout-mode": "none"
            }
        }
    },
    "protocol": {},
    "vlan": {
        "id": {
            "2": {
                "members": [
                    "ge-1"
                ]
            }
        }
    }
}' | jq

echo "Patch running config"
curl -s -X POST http://localhost:8001/config/update \
   -H 'Content-Type: application/json' \
   -d '[
  {
    "op": "remove",
    "path": "/interface/aggregate-ethernet/ae-1"
  },
  {
    "op": "remove",
    "path": "/interface/gigabit-ethernet/ge-2"
  },
  {
    "op": "add",
    "path": "/interface/gigabit-ethernet/ge-4",
    "value": {
      "speed": "100G"
    }
  },
  {
    "op": "add",
    "path": "/platform/port/ge-4",
    "value": {
      "breakout-mode": "none"
    }
  }
]'

echo "Get candidate config"
curl -s -X GET http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' | jq

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Apply candidate config"
curl -s -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' -d ''

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

# echo "Patch running config"
# curl -s -X POST http://localhost:8001/config/update \
#    -H 'Content-Type: application/json' \
#    -d '[
#   {
#     "op": "remove",
#     "path": "/interface/gigabit-ethernet/ge-1",
#     "value": {
#       "speed": "100G"
#     }
#   }
# ]'
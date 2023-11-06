curl -X POST http://localhost:8001/config/update \
   -H 'Content-Type: application/json' \
   -d '{
            "interface": {
               "gigabit-ethernet": {
                  "ge-5": {
                     "speed": "100G"
                  }
               }
            }
      }'

curl -X POST http://localhost:8001/config/diff \
   -H 'Content-Type: application/json' \
   -d '{
            "interface": {
               "gigabit-ethernet": {
                  "ge-5": {
                     "speed": "100G"
                  }
               }
            }
      }'

curl -X GET http://localhost:8001/config/get \
   -H 'Content-Type: application/json'

curl -X POST http://localhost:8001/config/diff \
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
}'
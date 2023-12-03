echo "Startup config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Post update good config [1]"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/ethernet/eth-2",
        "value": {
            "speed": "100G"
        }
    },
    {
        "op": "add",
        "path": "/platform/port/eth-2",
        "value": {
            "breakout-mode": "none"
        }
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/eth-2",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Apply candidate config"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Post update bad config (no removed /interface/ethernet/eth-2)"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/ae-1",
        "value": {
            "members": {
                "eth-2_1": null
            }
        }
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth-2_1",
        "value": {}
    },
    {
        "op": "replace",
        "path": "/platform/port/eth-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/id/2/members/eth-2"
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ae-1",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Post update bad config (no added /interface/ethernet/eth-2_1)"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/ae-1",
        "value": {
            "members": {
                "eth-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth-2"
    },
    {
        "op": "replace",
        "path": "/platform/port/eth-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/id/2/members/eth-2"
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ae-1",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Post update bad config (no updated /platform/port/eth-2/breakout-mode)"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/ae-1",
        "value": {
            "members": {
                "eth-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth-2_1",
        "value": {}
    },
    {
        "op": "remove",
        "path": "/vlan/id/2/members/eth-2"
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ae-1",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Post update bad config (no removed /vlan/id/2/members/eth-2)"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/ae-1",
        "value": {
            "members": {
                "eth-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth-2_1",
        "value": {}
    },
    {
        "op": "replace",
        "path": "/platform/port/eth-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ae-1",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Post update bad config (no created /interface/aggregate-ethernet/ae-1)"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "remove",
        "path": "/interface/ethernet/eth-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth-2_1",
        "value": {}
    },
    {
        "op": "replace",
        "path": "/platform/port/eth-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/id/2/members/eth-2"
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ae-1",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -ne 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Post update good config [2]"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/ae-1",
        "value": {
            "members": {
                "eth-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth-2_1",
        "value": {}
    },
    {
        "op": "replace",
        "path": "/platform/port/eth-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/id/2/members/eth-2"
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ae-1",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Rollback the changes"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Post update good config [3]"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '[
    {
        "op": "add",
        "path": "/interface/aggregate-ethernet/ae-1",
        "value": {
            "members": {
                "eth-2_1": null
            }
        }
    },
    {
        "op": "remove",
        "path": "/interface/ethernet/eth-2"
    },
    {
        "op": "add",
        "path": "/interface/ethernet/eth-2_1",
        "value": {}
    },
    {
        "op": "replace",
        "path": "/platform/port/eth-2/breakout-mode",
        "value": "4x100G"
    },
    {
        "op": "remove",
        "path": "/vlan/id/2/members/eth-2"
    },
    {
        "op": "add",
        "path": "/vlan/id/2/members/ae-1",
        "value": null
    }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Apply the changes"

HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request"
    exit 1
fi

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Successfully passed the test!"
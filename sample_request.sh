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

echo "Post update good config without session token"
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

if [ ${HTTP_STATUS} -eq 498 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config due to duplicated key at xpath '/interface/ethernet/eth-1'"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '[
    {
        "op": "add",
        "path": "/interface/ethernet/eth-1",
        "value": {
            "speed": "400G"
        }
    },
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
#         "path": "/interface/ethernet/eth-2",
#         "value": {
#             "speed": "100G"
#         }
#     },
#     {
#         "op": "add",
#         "path": "/platform/port/eth-2",
#         "value": {
#             "breakout-mode": "none"
#         }
#     },
#     {
#         "op": "add",
#         "path": "/vlan/id/2/members/eth-2",
#         "value": null
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

echo "Post update bad config (no removed /interface/ethernet/eth-2)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
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
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no added /interface/ethernet/eth-2_1)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
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
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no updated /platform/port/eth-2/breakout-mode)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
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
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no removed /vlan/id/2/members/eth-2)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
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
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update bad config (no created /interface/aggregate-ethernet/ae-1)"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
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
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update good config [2]"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
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

echo "Get running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Successfully passed the test!"
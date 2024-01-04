# webapp.py

import json
from functools import cached_property
from http.cookies import SimpleCookie
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qsl, urlparse
import os
import subprocess

ETH_INTF_PREFIX = "eth"
LAG_INTF_PREFIX = "ae"
PORT_XPATH = "/platform/port"
LAG_INTF_XPATH = "/interface/aggregate-ethernet"
VLAN_XPATH = "/vlan/id"

CALL_XRL = "/sbin/ip netns exec default /usr/bin/lib/libxipc/call_xrl -w 30"

def normalize_lag_ifname(ifname):
    ifname = ifname.replace("-", "")
    return ifname

def normalize_eth_ifname(ifname):
    ifname = ifname.replace("-", "/")
    ifname = ifname.replace("_", "/")
    ifname = "eth-1/1/{}".format(ifname[4:len(ifname)])
    return ifname

def process_create_lag(ifname):
    ifname = ifname.replace("-", "")
    print("Create LAG {}".format(ifname))
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/create_port?{}&ifname:txt={}\"".format(CALL_XRL, tid, ifname), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False

    return True

def process_delete_lag(ifname):
    ifname = ifname.replace("-", "")
    print("Delete LAG {}".format(ifname))
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/delete_port?{}&ifname:txt={}\"".format(CALL_XRL, tid, ifname), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False

    return True

def process_add_lag_member(lag_intf_xpath, member):
    ifname = lag_intf_xpath.split("/")[-2].replace("-", "")
    member = normalize_eth_ifname(member)
    print("Add member '{}' to LAG '{}'".format(member, ifname))
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/add_port_to_lag?{}&ifname:txt={}&lag_name:txt={}\"".format(CALL_XRL, tid, member, ifname), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False

    return True

def process_remove_lag_member(lag_intf_xpath, member):
    ifname = lag_intf_xpath.split("/")[-2].replace("-", "")
    member = normalize_eth_ifname(member)
    print("Add member '{}' to LAG '{}'".format(member, ifname))
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/remove_port_from_lag?{}&ifname:txt={}&lag_name:txt={}\"".format(CALL_XRL, tid, member, ifname), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False

    return True

def process_create_vlan(vlan):
    print("Create VLAN {}".format(vlan))
    p = subprocess.Popen("{} \"finder://sif/vlan_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/vlan_config/0.1/create_vlan_id?{}&vlan_id:u32={}\"".format(CALL_XRL, tid, vlan), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/vlan_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False

    return True

def process_delete_vlan(vlan):
    print("Delete VLAN {}".format(vlan))
    p = subprocess.Popen("{} \"finder://sif/vlan_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/vlan_config/0.1/delete_vlan_id?{}&vlan_id:u32={}\"".format(CALL_XRL, tid, vlan), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/vlan_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False

    return True

def process_add_vlan_member(vlan_xpath, member):
    vlan = vlan_xpath.split("/")[-2]
    if member.find(ETH_INTF_PREFIX) != -1:
        member = normalize_eth_ifname(member)
    elif member.find(LAG_INTF_PREFIX) != -1:
        member = normalize_lag_ifname(member)
    print("Add member '{}' to VLAN '{}'".format(member, vlan))
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/set_port_mode?{}&ifname:txt={}&mode:txt=trunk\"".format(CALL_XRL, tid, member), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False

    p = subprocess.Popen("{} \"finder://sif/vlan_config/0.1/add_port_to_vlan?ifname:txt={}&vlan_id:u32={}\"".format(CALL_XRL, member, vlan), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False

    return True

def process_remove_vlan_member(vlan_xpath, member):
    vlan = vlan_xpath.split("/")[-2]
    if member.find(ETH_INTF_PREFIX) != -1:
        member = normalize_eth_ifname(member)
    elif member.find(LAG_INTF_PREFIX) != -1:
        member = normalize_lag_ifname(member)
    print("Remove member '{}' from VLAN '{}'".format(member, vlan))
    p = subprocess.Popen("{} \"finder://sif/vlan_config/0.1/delete_port_from_vlan?ifname:txt={}&vlan_id:u32={}\"".format(CALL_XRL, member, vlan), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False

    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/set_port_mode?{}&ifname:txt={}&mode:txt=access\"".format(CALL_XRL, tid, member), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False

    return True

def process_port_breakout_mode(port_xpath, value):
    port = port_xpath.split("/")[-2]
    ifname = normalize_eth_ifname(port)
    breakout_mode = value
    if breakout_mode == "none":
        breakout_mode = "no"
    
    print("Set breakout-mode '{}' on the port {}".format(breakout_mode, ifname))
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/start_transaction\"".format(CALL_XRL), stdout=subprocess.PIPE, shell=True)
    (tid, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to start transaction: '{}'".format(err))
        return False
    
    tid = tid.decode("utf-8").strip()
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/set_port_split?{}&ifname:txt={}&split:txt={}\"".format(CALL_XRL, tid, ifname, breakout_mode), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to execute XRL: '{}'".format(err))
        return False
    
    p = subprocess.Popen("{} \"finder://sif/switch_port_config/0.1/commit_transaction?{}\"".format(CALL_XRL, tid), stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    if p_status != 0:
        print("Failed to commit transaction: '{}'".format(err))
        return False
    
    return True

def process_add_op(xpath, value):
    print("Processing ADD operation: xpath '{}', value '{}'".format(xpath, value))
    if xpath == LAG_INTF_XPATH:
        return process_create_lag(value)
    elif xpath.find(LAG_INTF_XPATH) != -1:
        return process_add_lag_member(xpath, value)
    elif xpath == VLAN_XPATH:
        return process_create_vlan(value)
    elif xpath.find(VLAN_XPATH) != -1:
        return process_add_vlan_member(xpath, value)
    elif xpath.find(PORT_XPATH) != -1 and xpath.find("breakout-mode") != -1:
        return process_port_breakout_mode(xpath, value)

    return True

def process_replace_op(xpath, value):
    print("Processing REPLACE operation: xpath '{}', value '{}'".format(xpath, value))
    if xpath.find(PORT_XPATH) != -1 and xpath.find("breakout-mode") != -1:
        return process_port_breakout_mode(xpath, value)

    return True

def process_remove_op(xpath, value):
    print("Processing REMOVE operation: xpath '{}', value '{}'".format(xpath, value))
    if xpath == LAG_INTF_XPATH:
        return process_delete_lag(value)
    elif xpath.find(LAG_INTF_XPATH) != -1:
        return process_remove_lag_member(xpath, value)
    elif xpath == VLAN_XPATH:
        return process_delete_vlan(value)
    elif xpath.find(VLAN_XPATH) != -1:
        return process_remove_vlan_member(xpath, value)
    elif xpath.find(PORT_XPATH) != -1 and xpath.find("breakout-mode") != -1:
        return process_port_breakout_mode(xpath, value)

    return True

class WebRequestHandler(BaseHTTPRequestHandler):
    @cached_property
    def url(self):
        return urlparse(self.path)

    @cached_property
    def query_data(self):
        return dict(parse_qsl(self.url.query))

    @cached_property
    def post_data(self):
        content_length = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(content_length)

    @cached_property
    def form_data(self):
        return dict(parse_qsl(self.post_data.decode("utf-8")))

    @cached_property
    def cookies(self):
        return SimpleCookie(self.headers.get("Cookie"))

    def do_PUT(self):
        print("Hello from PUT method")

    def do_DELETE(self):
        print("Hello from DELETE method")

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(self.get_response().encode("utf-8"))

    def do_POST(self):
        jconfig = json.loads(str(self.post_data.decode("utf-8")))
        print(json.dumps(jconfig, indent=4))
        print("Operation: {}".format(jconfig["op"]))
        print("Path: {}".format(jconfig["path"]))
        print("Value: {}".format(jconfig["value"]))
        if jconfig["op"] == "add":
            if process_add_op(jconfig["path"], jconfig["value"]):
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(self.get_response().encode("utf-8"))
        elif jconfig["op"] == "replace":
            if process_replace_op(jconfig["path"], jconfig["value"]):
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(self.get_response().encode("utf-8"))
        else:
            if process_remove_op(jconfig["path"], jconfig["value"]):
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(self.get_response().encode("utf-8"))

    def get_response(self):
        return json.dumps(
            {
                "path": self.url.path,
                "query_data": self.query_data,
                "post_data": self.post_data.decode("utf-8"),
                "form_data": self.form_data,
                "cookies": {
                    name: cookie.value
                    for name, cookie in self.cookies.items()
                },
            }
        )

if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", 8000), WebRequestHandler)
    server.serve_forever()
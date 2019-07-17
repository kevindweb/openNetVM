from paramiko import RSAKey
from paramiko import SSHClient
from paramiko import AutoAddPolicy
import sys
import time
import datetime

if len(sys.argv) != 6:
    print("ERROR! Incorrect number of arguments")
    sys.exit(1)

worker_ip = sys.argv[1]
key_file = sys.argv[2]
worker_user = sys.argv[3]
dpdk_ip_addr = sys.argv[4]
run_time = sys.argv[5]

key = RSAKey.from_private_key_file(key_file)

client = SSHClient()
client.set_missing_host_key_policy(AutoAddPolicy())
client.connect(worker_ip, timeout = 30, username=worker_user, pkey = key)

epwget = "sudo ~/mtcp_client {} {}".format(dpdk_ip_addr, run_time)
(stdin, stdout, stderr) = client.exec_command(epwget, get_pty=True)

# block until script finishes
exit_status = stdout.channel.recv_exit_status()

# write to file for debugging 
with open('/home/' + worker_user + '/paramiko_mtcp_out.log', 'a+') as paramiko_out:
    paramiko_out.write(datetime.datetime.now().strftime("%I:%M%p on %B %d, %Y") + "\n")
#    paramiko_out.write(stdout.read().decode('ascii') + "\n")

# kill client on other server
(stdin, stdout, stderr) = client.exec_command("sudo pkill epwget", get_pty=True)
print("Successfully ran mTCP".format(worker_ip))
client.close()

import pexpect
import os

cwd = os.path.dirname(os.path.realpath(__file__))
# this script asks for user input, tell it yes to bind dpdk0
child = pexpect.spawn(cwd + "/mtcp/setup_mtcp_onvm_env.sh", cwd=cwd)

child.timeout = 30
# allow mtcp to bind the correct interface
child.expect("Are you using an Intel NIC (y/n)?.*")
child.sendline("y\n")
child.interact()

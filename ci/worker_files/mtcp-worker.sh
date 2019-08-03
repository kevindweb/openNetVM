#!/bin/bash              

log "Binding mTCP interface"                       
if [[ ! -z $MTCP_IFACE ]]                          
then                     
    # running mTCP, set up interfaces              
    bind_nic_from_iface $MTCP_IFACE                
    python3 ~/mtcp-bind.py                         
    sudo ifconfig dpdk0 $MTCP_IP_ADDR/24 up        
else                     
    echo "ERROR: mTCP interface was not provided"  
    exit 1               
fi                       

log "Running ONVM Manager"                         
cd ~/repository/onvm     
# run manager with only port 1 for mTCP's dpdk0 interface                                             
./go.sh 0,1,2,3 2 0xF0 -a 0x7f000000000 -s stdout &>~/onvm_mtcp_stats &                               
mgr_pid=$?               
if [ $mgr_pid -ne 0 ]    
then                     
    echo "ERROR: Starting manager failed"          
    exit 1               
fi                       

# wait for the manager to come online              
sleep 15                 
log "Manager is live"    

# go to mtcp directory   
cd ~/mtcp/apps/example   

log "Running EP Server"  
sudo ./epserver -p ~/www -f ./epserver.conf -N 1 &>~/server_stats &                                   
epserver_pid=$?          
if [ $epserver_pid -ne 0 ]                         
then                     
    echo "ERROR: Starting EP Server failed"        
    return 1             
fi                       

# wait for server to come online                   
log "Waiting for EP server to go live"             
sleep 10                 

log "Running EP Wget"    
# run ep wget on worker  
python3 ~/run-mtcp.py $WORKER_IP $WORKER_KEY_FILE $WORKER_USER $MTCP_IP_ADDR $MTCP_RUN_TIME           
scp -i $WORKER_KEY_FILE -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null $WORKER_USER@$WORKER_IP:~/client_stats ~/mtcp_client_stats

log "Killing EP Server"  
sudo pkill -f epserver   
check_exit_code "ERROR: Killing EP server failed"  

log "Exiting Manager"    
sudo pkill -f onvm_mgr   
check_exit_code "ERROR: Killing manager failed"


Startup & Shutdown procedure.

 I.  Startup
   Power on at cabinet level, the one-control box startup automatic.
   When "startup" event trigged, do follow step, and output the current status into start_status.tmp,
   (1) Check disk array (about 300s).
       1) before check, output init status
            1489457774 1 0            (0:init)
       2) do checking, output:
            1489457785 1 1            (1:checking)
       3) check completed, output:
          when disk array ready:
            1489457785 1 2            (2:pass)
          when disk array failed:
            1489457785 1 3            (3:fail)

   (2) Startup the phycial management host(Primary server)(about 300s).
       1) before startup, check server status, 
          if not running:
            1489457774 2 0 srv01           (0:init)
          if running, go to step(3)
            1489457774 2 2 srv01           (2:running)
       2) do startup through ipmitool, output:
            1489457785 2 1 srv01           (1:starting)
       3) check server status using ping, output:
          when server ready:
            1489457785 2 2 srv01           (2:running)
          when server failed:
            1489457785 2 3 srv01           (3:fail)
   
   (3) Startup the phycial compute host(Secondary server,may be more than one). (about 300s per host)
       some operation execute in parallel.
       1) before startup, check server status, 
          if not running:
            1489457774 3 0 srvN           (0:init)
          if running, go to step(3) for startup next compute host.
            1489457774 3 2 srvN           (2:running)
       2) do startup through ipmitool, output:
            1489457785 3 1 srvN           (1:starting)
       3) check server status using ping, output:
          when server ready:
            1489457785 3 2 srvN           (2:running)
          when server failed:
            1489457785 3 3 srvN           (3:fail) (need to retry)

   (4) Startup cloud platform.(about 300s)
       1) before startup, check all servers status, all servers must be in running, 
          if not pass:
            1489457774 4 -1           (-1:not ready)
          if running, go to step(3)
            1489457774 4 0            (0:init)
       2) execute cloud platform startup shell script, output:
            1489457785 4 1           (1:starting)
       3) check cloud platform status, output:
          when platform ready:
            1489457785 4 2            (2:running)
          when platform startup failed:
            1489457785 4 3            (3:fail)(need to retry)
    
     after all startup step executed, output summary:
          when success:
            1489457785 0 1 startup    (1:success)
          when failure:
            1489457785 0 0 startup    (0:failure)





 II.  Shutdown
   When "shutdown" event trigged, do follow step, and output the current status into stop_status.tmp,
   (1) Shutdown the application. (about 60s)
       1) before shutdown, check all servers status, all servers must be in running,
          if running:
            1489457774 1 0           (0:init)
          if not running, go to step(2)
            1489457774 1 2           (2:stopped)
       2) execute application shutdown shell script, output:
            1489457785 1 1           (1:stopping)
       3) check application status, output:
          when application stopped:
            1489457785 1 2            (2:stopped)
          when application shutdown failed:
            1489457785 1 3            (3:fail)(need to retry)

   (2) Shutdown the system VM.(may be more than one VM)(about 150s)
       1) before shutdown, check the VM status using ping or ssh,
          if running:
            1489457774 2 0 vmN          (0:init)
          if not running, go to step(3)
            1489457774 2 2 vmN           (2:stopped)
       2) shutdown the VM using ssh, before execute shutdown, output:
            1489457785 2 1 vmN          (1:stopping)
       3) check VM status, output:
          when VM stopped:
            1489457785 2 2 vmN           (2:stopped)
          when VM shutdown failed:
            1489457785 2 3 vmN           (3:fail)(need to retry)

   (3) Shutdown the phycial compute host(Secondary server,may be more than one).(about 300s per host)
       some operation execute in parallel.
       1) before shutdown, check server status, 
          if running:
            1489457774 3 0 srvN           (0:init)
          if not running, go to step(4) for shutdown next compute host.
            1489457774 3 2 srvN           (2:stopped)
       2) do shutdown through ipmitool, output:
            1489457785 3 1 srvN           (1:stopping)
       3) check server status using ping, output:
          when server stopped:
            1489457785 3 2 srvN           (2:stopped)
          when server shutdown failed:
            1489457785 3 3 srvN           (3:fail) (need to retry)

   (4) Shutdown the phycial management host(Primary server)(about 300s).
       1) before shutdown, check server status, 
          if running:
            1489457774 4 0 srv01           (0:init)
          if not running.
            1489457774 4 2 srv01           (2:stopped)
       2) do shutdown through ipmitool, output:
            1489457785 4 1 srv01           (1:stopping)
       3) check server status using ping, output:
          when server stopped:
            1489457785 4 2 srv01           (2:stopped)
          when server shutdown failed:
            1489457785 4 3 srv01           (3:fail) (need to retry)

     after all shutown step executed, output summary:
          when success:
            1489457785 0 1 shutdown    (1:success)
          when failure:
            1489457785 0 0 shutdown    (0:failure)


 III.  Status definition.
   format:  <timestamp> <step_num> <device status code> <server_id>



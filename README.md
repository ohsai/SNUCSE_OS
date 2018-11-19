# osfall2018-team3  
  
syscall number 384 is owned by other one so do not use it  
terminating while holding lock does not release lock, this is current problem.  
Current version [ 11/17 5:00 pm ] is not a rwlock, but few change will make it as rwlock.  

rotd daemon, selector and trial  
```console
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/rotd.c -o ./test/rotd  
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/selector.c -o ./test/selector
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/trial.c -o ./test/trial

sh4.2$ ./rotd
sh4.2$ ./selector [number] & ./trial 1 & ./trial 2 & ./trial 3 & 
sh4.2$ dmesg | tail -n 20
sh4.2$ killall trial && killall selector

WHY IS THIS CRAZY??? -- TRY TO DO

selector 28374 & ./trial 1 & ./trial 2 & ./trial 3 &
dmesg | grep "lookup\|set_rotation"

```  

selector / trial example works well currently.  


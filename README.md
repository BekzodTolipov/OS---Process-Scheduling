# Process Scheduling

	To run the program:

	1. Type: "make"
	2. Run executable: "./oss"
	3. For help text type: ./oss -h
	4. Extra options:
		./oss -l : specifies the log file (default is log.dat).
		./oss -t : specifies the timer taht alarm will go off.

## Description
In this project, you will simulate the process scheduling part of an operating system. You will implement time-based scheduling,
ignoring almost every other aspect of the OS. In this project we will be using message queues for synchronization.
*NOTE: this is simulation, there will be no "real-time waiting". Time result WILL BE inconsistence or divergence.
*NOTE: to avoid confusion, I will be working with second and nanosecond ONLY.

The operating system simulator, or OSS, will be your main program and serve as the master process. 
You will start the simulator (call the executable oss) as one main process who will fork multiple children at random times. 
The randomness will be simulated by a logical clock that will also be updated by oss.

Assuming you have more than one process in your simulated system, oss will select a process to run and schedule it for execution. It
will select the process by using a scheduling algorithm.

Expected output will as following:

	OSS: Generating process with PID 3 and putting it in queue 1 at time 0:5000015
	OSS: Dispatching process with PID 2 from queue 1 at time 0:5000805,
	OSS: total time this dispatch was 790 nanoseconds
	OSS: Receiving that process with PID 2 ran for 400000 nanoseconds
	OSS: Putting process with PID 2 into queue 2
	OSS: Dispatching process with PID 3 from queue 1 at time 0:5401805,
	OSS: total time this dispatch was 1000 nanoseconds
	OSS: Receiving that process with PID 3 ran for 270000 nanoseconds,
	OSS: not using its entire time quantum
	OSS: Putting process with PID 3 into queue 1
	OSS: Dispatching process with PID 1 from queue 1 at time 0:5402505,
	OSS: total time spent in dispatch was 7000 nanoseconds
	etc...


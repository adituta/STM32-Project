# STM32-Project
In this repository I will upload all the research and development of a personal project on STM32 platform.



Hello evryone!
Happy New Year!

I've started documenting about the core concepts that I really need to know in order to bring this project to reality.
I've done some research about the communication protocols needed in this project such as SPI, I2C and UART, usecases when I need them, advantages and disadvantages in using them.
The YouTube video that opened my eyes can be accessed at: https://www.youtube.com/watch?v=IjjZcnwkrAg&t=492s
The documents that dive deep into every protocol can be accessed via the links below:
- SPI: https://www.analog.com/media/en/analog-dialogue/volume-52/number-3/introduction-to-spi-interface.pdf
- I2C: https://www.ti.com/lit/an/slva704/slva704.pdf?ts=1710888631958
- UART: https://www.analog.com/media/en/analog-dialogue/volume-54/number-4/uart-a-hardware-communication-protocol.pdf

Also, during the weeks since creating this repository I researched the reasons why an RTOS is well suited in accomplishing certain tasks. An RTOS is absolutely necessary in managing critical tasks and fine grained processing that have as results adjusting real-time systems (adjusting the fuel-to-air ratio in internal combustion engines by on demand necesities of the driver and the road, keeping a thermal powerplant running by permanently keeping track of the boiler and turbine parameters (temperature, pressure, synchronization with the grid, coal supply, emergency situations).

Of course, I will not start to build my own powerplant or car and find use for the RTOS. This operating systems are really well fitted in probes that gather data from the environment (for civil or military applications). As the project name suggests, my project has to collect data from the nature, do some processing on it, extrat some valuable insight and then send it to the master.

During this holyday, I received a semester project at Digital Signal Processing Course in which I had to think about a problem in which I can apply concepts of Frecquency Analysis in order to solve it. I thought that an RTOS can sense the enemy forces advancing and can determine the direction of fire from the enemy. This can be achieved by having an array of microphonnes through which the support can triangulate the direction af attacking troops. In addition, with other sensors on board the support can determine the enemy's position more precisely.
- Link to the DSP homework: 

Another developing ways is to make a system that can caputure images from time to time and detect abnormalities. Also, an autofocus system can be done by adjusting the image around a point of interest (mechanically adjusting the lenses or sharpening the image digitally).

Alos, I've watched a video on how to use the CubeMX and CubeIDE and I'll follow a course on getting used to the features and environment offered by the STM32 platform.

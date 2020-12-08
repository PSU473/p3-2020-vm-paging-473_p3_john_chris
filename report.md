Project Members: John Rost and Chris Muller

Report Author: John Rost

  For our third and final project of the semester, we were tasked with implementing an access control mechanism which would allow for the hardware to handle finding
pages which are already in memory, alongside the implementation of two separate replacement algorithms for new pages of memory: FIFO and TC. Both Chris and I 
spent a great deal of time researching these topics before beginning to implement our code. The most difficult task initially was getting used to the new symantics
and function calls we would be using throughout the project, such as mprotect, sigaction, etc. I focused mainly on researching and implementing the FIFO functionality,
while Chris focused more on Third Chance. We both communicated with each other frequently while coming up with ideas for how to tackle the problems that we were facing,
and each of us had a hand in designing our code. The biggest problem we had was correctly implementing the fault_types for our mm_logger calls; we had a great deal of 
difficulty in properly getting each fault type to correctly display all of the time, and because of the inconsistency with our original implementation of typing, we 
decided to cut most of it out. More often than not, we opted to just leave the types at 0. Our code's structure follows a fairly logical path. We have a fifo and tc
struct, which allow us to store page information and create circularly linked lists. Our mm_innit function makes a choice between using our fifo algorithm or our
tc one, depending on what policy is passed as input. In both our fifo and tc algorithms, we first check if the page is already within physical memory. After that, we 
simply need to handle the corresponding faults and procedures for when the pages are not already contained within the physical memory. This project took
a great deal of time spent on research and troubleshooting various kinds of errors, and both Chris and I are very proud of the work we accomplished.

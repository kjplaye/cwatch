c(olorized) watch
------------------

Like watch, but colorize changes according to the time since the last change.  Use edit distance to find out which characters have changed.  An edit, an insert, or a delete count as a change.  Changes are represented by colors:

    RED  : Change occured     1 attempt  ago, (just changed)
 YELLOW  : Change occured    10 attempts ago.
  GREEN  : Change occured   100 attempts ago.
   CYAN  : Change occured  1000 attempts ago.
MAGENTA  : Change occured 10000 attempts ago.

Example output for: cwatch date

![alt text](https://github.com/kjplaye/cwatch/blob/master/example_output_1.png?raw=true)

Example output for: cwatch "cat /proc/meminfo | head"

![alt text](https://github.com/kjplaye/cwatch/blob/master/example_output_2.png?raw=true)


OPTIONS
--------

-c : Don't clear the terminal after each command.

-d : You can change the default delay between commands.  Note that bigger strings will place higher demands on the CPU (and memory).  So there is a lower bound to how fast commands are executed.

-s -w -h : These values are max_string, window_size and history.  They default to 10000, 400, and 10000 respectively.  Set them lower or higher to use less or more resources.

cwatch = c(olorized) watch
---

Like watch, but colorizes changes according to the time since the last change.  
   * A replacement, an insert, or a delete count iss a change.  
   * Use edit distance to find out which characters have changed.  
   * Changes are represented by colors.

## Color Meaning

| Option | Description | |
| ------ | ----------- |---|
| RED    | Change occurred | 1 attempt ago, (just changed) |
| YELLOW    | Change occurred | 10 attempts ago |
| GREEN    | Change occurred | 100 attempts ago |
| CYAN   | Change occurred | 1000 attempts ago |
| MAGENTA | Change occurred | 10000 attempts ago |

## Example Outputs
**cwatch date**

![alt text](https://github.com/kjplaye/cwatch/blob/master/example_output_1.png?raw=true)

**cwatch "cat /proc/meminfo | head"**

![alt text](https://github.com/kjplaye/cwatch/blob/master/example_output_2.png?raw=true)

## Installation

Just run **make**.

## Options

-c : Don't clear the terminal after each command.

-d : You can change the default delay between commands.  Note that bigger strings will place higher demands on the CPU (and memory).  So there is a lower bound to how fast commands are executed.

-s -w -h : These values are max_string, window_size and history.  They default to 10000, 400, and 10000 respectively.  Set them lower or higher to use less or more resources.

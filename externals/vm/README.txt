vm: a pay-as-you-go, coin operated virtual machine

input: float to tell how many operations to run before the machine suspends

output: any change left after running the virtual machine

think about a secondary output: for the output returned by the script
loop;

arg0: OP_STARTLOOP
arg1: variable or const value to hold the number of iterations
arg2: warp zone (index of endloop)
arg3: nothing

end loop;

arg0: OP_ENDLOOP
arg1: variable or const value to hold the number of iterations. For the
      loop ender this is not used.
arg2: warp zone (index of start of loop)
arg3: numeric counter, initially set to arg1 value. But for the loop
      starter this is not actually used. In parser this is just set to zero

if:
arg0: OP_IF
arg1: variable or const to hold the boolean
arg2: warp zone to else clause or to the rest of the code
arg3: const if we have an else clause or not

else:
arg0: OP_ELSE
arg1: const to hold the negation of our OP_IF value
arg2: warp zone to the next thing after the else clause
arg3: not needed

endif: no op for this
pop the last if or else off the stack, set our index to its arg2
warp zone


loop i 0 pointer-to-one-past-the-end-of-the-loop;

end loop pointer-to-one-past-the-beginning-of-the-loop;



if condition pointer-to-else

else if-condition-value-result pointer-to-first-op-after-the-else


what needs to happen?

set the ops


In this lab i first started by working on the make mount and unmount fs functions. The make_fs was fairly straight forward and once i figured out mount_fs, unmount was pretty easy. 

Then I went on to work on the close, create and delte functions. I had a bit of difficulty with the delete but was able to get it to work after some debugging.

The two functions that gave me the most trouble were the read and write, but after figuring out all the arithmetics with the offsets I was able to get them working. The last function that also gave me trouble was the truncate function, but again it was just making sure that the arithmetics were correct and hours of debugging to find the mistakes.

I also used a few helper functions to make the code clearer which were find_file_spot, reset_descriptors, get_next, find_free_block, set_first_superblock, and clear_data.

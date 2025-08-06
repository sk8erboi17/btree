The B-tree is designed for disk-based operations, where nodes are read from and written to a single file. This simulates how a database would interact with a hard drive.
The program will automatically create or load a file named extreme_btree.db in the same directory and execute a series of tests to demonstrate the B-tree's functionality.

You will see output detailing the creation, insertion, and search operations, along with a visual representation of the tree's structure after each major operation. The file extreme_btree.db will be created and modified by the program.
The project includes a series of test cases to demonstrate functionality, including handling of special values, stress tests with sequential and reverse insertions, and operations on empty or single-node trees.

This implementation follows the B-tree algorithm described in the Cormen's "Introduction to Algorithms," but with a key difference: it's designed for disk-based operations.

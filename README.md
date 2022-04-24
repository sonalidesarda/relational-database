# 5300-Hyena
CPSC 5300 Sprint Verano, Seattle University, Spring 2022

Author: Han Duong
References heap_storage_test.cpp: Luan(Remi) Ta - Mink 

Milestone1:
This milestone is a program written in C++ that runs from the command line, which take in user input and parse it to formatted SQL statement.

Build the program, use : $ make

Run the program, use: $ ./Verano env_path 

Env_path: The directory where the Berkeley DB database files reside in.

The terminal will when run shows: 
SQL>

Exit the program, use: SQL> quit

Milestone2:
This milestone is a program written in C++ that mimics a storage engine. It includes: DbBlock, DbFile, and DbRelation and their represetation in the heap storage engine: SlottedPage, HeapFile, and HeapTable.

Build the program, use : $ make test

Run this program with tests, enter: $ ./test out

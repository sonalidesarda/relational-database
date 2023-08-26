# Ralational Database Management System
Built relation database management system using file primitives from the Berkeley DB library. 

To build the program:
`$ make`

To run the program:
`$ ./sql5300 ~/cpsc5300/data`

_Note - Make sure BerkleyDB library is present on the device._

## This project includes rudimentary implementation of a Schema Storage that support the following commands:
* CREATE TABLE
#### Syntax:
```
CREATE TABLE <table_name> (<column_definitions>)
```
* DROP TABLE
#### Syntax:
```
DROP TABLE <table_name>
```
* SHOW TABLES
#### Syntax:
```
SHOW TABLES
```
* SHOW COLUMNS
#### Syntax:
```
SHOW COLUMNS FROM <table_name>
```
* CREATE INDEX
#### Syntax:
```
CREATE INDEX index_name ON table_name [USING {BTREE | HASH}] (col1, col2, ...)
```
* SHOW INDEX
#### Syntax:
```
SHOW INDEX FROM table_name
```
* DROP INDEX
#### Syntax:
```
DROP INDEX index_name FROM table_name
```
* INSERT ROW
#### Syntax:
```
INSERT INTO table (col_1, col_2, col_n) VALUES (1, 2, "three")
```
* DELETE ROW
#### Syntax:
```
DELETE FROM table WHERE col_1 = 1
```
* SELECT
#### Syntax:
```
SELECT col_1, col_2 FROM table
```
### Usage:
Clean the builds:
<br />`$ make clean`
<br />Build project:
<br />`$ make`
<br />After compiling, run the following command to start the SQL shell:
<br />`$ ./sql5300 [PATH]/data`
<br />To test the storage engine, use the `test` command:
<br />`$ SQL> test`
<br />To exit the SQL shell, use the `quit` command:
<br />`$ SQL> quit`

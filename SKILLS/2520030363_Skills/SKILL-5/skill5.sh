#!/bin/bash

echo "===== SKILL 5: QUOTING IN BASH ====="

name="Akshay"
text="Hello World"

echo
echo "1. SINGLE QUOTES"
single='Hello $name'
echo "Single quoted string: $single"

echo
echo "2. DOUBLE QUOTES"
double="Hello $name"
echo "Double quoted string: $double"

echo
echo "3. PRESERVE SPACES"
message="Hello World from Ubuntu"
echo "Message: $message"

echo
echo "4. LITERAL CONTENT"
literal='This is $name'
echo "Literal content: $literal"

echo
echo "5. VARIABLE EXPANSION"
expanded="This is $name"
echo "Expanded content: $expanded"

echo
echo "6. QUOTED STRING STORAGE"
quoted_string='Ubuntu Linux Bash'
echo "Stored string: $quoted_string"

echo
echo "7. EDGE CASE TEST"
special='Special characters: $ @ # ! *'
echo "Single quotes: $special"

special2="Special characters: \$ @ # ! *"
echo "Double quotes: $special2"

echo
echo "8. QUOTED COMMAND"
command_output="$(echo "Hello from command")"
echo "Command output: $command_output"

echo
echo "===== TEST COMPLETED ====="

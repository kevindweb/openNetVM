#! /bin/bash

# A script to run linter functions on updated files.
# This script is triggered by github actions when pull request are created.

input="files.txt"
diff_output="diff.txt"
user_lint="file1.txt"
master_lint="file2.txt"

# Get the name of the files that have been updated by the user.
git remote add upstream https://github.com/kevindweb/openNetVM.git
git fetch upstream
git diff --name-only upstream/develop...HEAD -- '*.c' '*.cpp' '*.h' | grep -v "cJSON" | grep -v "ndpi" > $input

# Run lint on each file updated by the user.
while IFS= read -r user_file
do
    python style/gwclint.py $user_file 2>> $user_lint
    echo "Done processing $user_file" >> $user_lint
done < "$input"

# Run lint on files in the master branch.
git stash save "Saved user's files"
git checkout upstream/master
while IFS= read -r master_file
do
    python style/gwclint.py $master_file 2>> $master_lint
    echo "Done processing $master_file" >> $master_lint
done < "$input"
git stash pop

# Only output errors present in the user's changes and not in the master's lint.
grep -v -F -x -f $master_lint $user_lint > $diff_output
echo $'\n Your lint results below. \n'
cat $diff_output

# Check if the diff.txt file is empty. If it's not empty, errors are present and the lint exits 1.
if [[ -s $diff_output ]]; then
    exit 1
else
    exit 0
fi ;

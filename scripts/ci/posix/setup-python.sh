#!/bin/bash

# Check if the input path is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <target-path>"
    exit 1
fi

TARGET_PATH="$1"

# Define the source path of the pyenv Python version
SOURCE_PATH="$HOME/.pyenv/versions/3.11.8"

# Check if the source path exists
if [ ! -d "$SOURCE_PATH" ]; then
    echo "Error: Source path $SOURCE_PATH does not exist."
    exit 1
fi

# Create the target path if it does not exist
mkdir -p "$TARGET_PATH"

# Copy the pyenv Python version to the target path
cp -r "$SOURCE_PATH" "$TARGET_PATH"

# Verify if the copy was successful
if [ $? -eq 0 ]; then
    echo "Successfully copied Python 3.11.8 to $TARGET_PATH"
else
    echo "Error: Failed to copy Python 3.11.8."
    exit 1
fi
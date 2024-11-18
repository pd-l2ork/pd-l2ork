#!/bin/bash
FILE_PACKAGER=`dirname \`which emcc\``/tools/file_packager.py

# Packages all files in a chunk into a .data file
process_chunk() {
    $FILE_PACKAGER main-$2.data --js-output=main-$2.js --preload $1 --quiet --no-node --use-preload-plugins
}

# Function to list files into chunks
create_chunks() {
    local input_dir="$1"
    local max_size=26214400  # 25MB in bytes
    local current_chunk_size=0
    local current_chunk=""
    local chunk_id=0

    while IFS= read -r file; do
        file_size=$(stat -c%s "$file")
        
        # Check if adding this file would exceed the current chunk size
        if (( current_chunk_size + file_size > max_size )); then
            # Process the current chunk and reset for the next chunk
            process_chunk "$current_chunk" $chunk_id
            current_chunk=""
            current_chunk_size=0
            chunk_id=$((chunk_id + 1))
        fi
        
        # Add the file to the current chunk
        current_chunk+="$file "
        current_chunk_size=$((current_chunk_size + file_size))
    done < <(find "$input_dir" -type f)
    
    # Process any remaining files in the last chunk
    if [ -n "$current_chunk" ]; then
        process_chunk "$current_chunk" $chunk_id
    fi
}

# Check for input arguments
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <directory> <js to append to>"
    exit 1
fi

# Start processing files in chunks
create_chunks $1
cat main-*.js >> $2
rm main-*.js
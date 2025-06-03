#!/bin/bash

# Input files
CERT_FILE="path-to-certificate.pem.crt"
KEY_FILE="path-to-private.pem.key"
# Output directory and file
OUTPUT_DIR="examples/demo_config"
OUTPUT_FILE="${OUTPUT_DIR}/formatted_certificate_and_key.txt"

# Macro names
CERT_MACRO="AWS_IOT_THING_CERT"
KEY_MACRO="AWS_IOT_THING_PRIVATE_KEY"

# Function to write PEM file into macro format
write_macro() {
    local file="$1"
    local macro_name="$2"

    if [[ ! -f "${file}" ]]; then
        echo "Error: ${file} not found."
        exit 1
    fi

    echo "#define ${macro_name} \\" >> "${OUTPUT_FILE}"

    while IFS= read -r line || [[ -n "${line}" ]]; do
        escaped_line=$(echo "${line}" | sed 's/\\/\\\\/g; s/"/\\"/g')
        echo "    \"${escaped_line}\\n\"\\" >> "${OUTPUT_FILE}"
    done < "${file}"

    # Remove the trailing backslash from the last line of this macro
    sed -i '' -e '$ s/\\$//' "${OUTPUT_FILE}"
}

# Create the output directory if it doesn't exist
mkdir -p "${OUTPUT_DIR}"

# Clear the output file
> "${OUTPUT_FILE}"

# Write certificate
write_macro "${CERT_FILE}" "${CERT_MACRO}"
echo "" >> "${OUTPUT_FILE}"

# Write private key
write_macro "${KEY_FILE}" "${KEY_MACRO}"

echo "Formatted certificate and private key written to ${OUTPUT_FILE}"

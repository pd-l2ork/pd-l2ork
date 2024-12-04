OS=$(uname)
REPO=$(git remote get-url origin)

if [ -z "$TOKEN" ]; then
    read -p "Enter GitHub Runner Token: " TOKEN
fi

# Function to check if a command exists
ensure_command() {
    local cmd="$1"
    if ! which "$cmd" &>/dev/null; then
        echo "Error: '$cmd' not found. Please install it before proceeding." >&2
        exit 1
    fi
}

if [ $OS == "Linux" ]; then
    ensure_command docker-compose
    ensure_command docker

    if [[ "$REPO" =~ ^git@([^:]+):(.+)$ ]]; then
        REPO="https://${BASH_REMATCH[1]}/${BASH_REMATCH[2]}"
    fi
    REPO="${REPO%.git}"

    REPO=$REPO TOKEN=$TOKEN docker-compose up
elif [ $OS == "Darwin" ]; then
    # TODO
    echo "TODO"
else
    echo "OS Not supported!"
fi
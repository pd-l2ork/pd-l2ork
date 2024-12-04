#!/usr/bin/env /bin/bash
if ([ -d /runner ] && [ "$(ls -A /runner 2>/dev/null)" = "" ]) || [ ! -f /runner/.credentials ]; then
    cp -r /runner-boostrap/* /runner

    cd /runner
    ./config.sh --unattended --url $REPO --token $TOKEN --name linux
else
    cd /runner
fi

./run.sh
#!/usr/bin/env bash

GH_TOKEN=""
GH_USER=""
URL="https://api.github.com/repos/SoPra-Team-17/Server/issues"
if [ $EXIT_STATUS -ne 0 ]; then
    LOG=$(journalctl _SYSTEMD_INVOCATION_ID="$INVOCATION_ID")
    MESSAGE="{\"title\":\"Crash: $(date)\", \"body\": \"<details>
<summary>Log</summary>

\`\`\`
${LOG//\"/\\\"}
\`\`\`
</details>\"}"

    curl \
        -X POST \
        -u "$GH_USER:$GH_TOKEN" \
        -H "Accept: application/vnd.github.v3+json" \
        "$URL" \
        -d "${MESSAGE//$'\n'/\\n}"
fi


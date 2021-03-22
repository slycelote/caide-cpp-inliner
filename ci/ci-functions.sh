
ansi_red="\e[0;91m"
ansi_blue="\e[0;94m"
ansi_reset="\e[0m"

function ci_timer {
    echo -e "${ansi_blue}    Current time: $(date +%H:%M:%S) ${ansi_reset}"
}

# Original code at https://github.com/travis-ci/travis-build/blob/52d064e1e1db237633ac9e9417d6c0aa447c97ed/lib/travis/build/bash/travis_retry.bash
# under MIT license
function ci_retry {
  local result=0
  local count=1
  while [[ "${count}" -le 3 ]]; do
    [[ "${result}" -ne 0 ]] && {
      echo -e "\\n${ansi_red}The command \"${*}\" failed. Retrying, ${count} of 3.${ansi_reset}\\n" >&2
    }
    # run the command in a way that doesn't disable setting `errexit`
    "${@}"
    result="${?}"
    if [[ $result -eq 0 ]]; then break; fi
    count="$((count + 1))"
    sleep 1
  done

  [[ "${count}" -gt 3 ]] && {
    echo -e "\\n${ansi_red}The command \"${*}\" failed 3 times.${ansi_reset}\\n" >&2
  }

  return "${result}"
}


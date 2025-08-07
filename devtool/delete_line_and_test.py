"""
Mutation Testing Script

Originally written by Gemini. Will try to delete 1 line at a time from the given file,
and keep the change if it does not cause a regression (detected by failure count).

The original motivation for this is that Claude outputs a massive soup of code,
some of which may not actually be necessary. Considering minifying the code is
something we can do in a basic automated loop (this program!) it would be a waste of tokens
to ask the AI to do it. I recommend running this on a clean git state and reviewing
the changes with git diff after, since it will probably delete comments, line breaks,
and other things you want to keep.

AI or not, sometimes you might be surprised with what code you don't need.
Or at least, what code you can apparently get away with not having.

Minimum Python Version: 3.8
"""
import subprocess
import re
import sys
import time
from pathlib import Path
import logging

# Configure logging to show information about the process.
# This helps in tracking progress, successful/failed deletions, and any issues.
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def run_command(command: list[str], cwd: Path | None = None) -> tuple[int, str]:
    """
    Runs a shell command and returns the number of failed tests and the full output.
    This function assumes the test command will produce output that can be parsed
    to determine the number of test failures, specifically looking for pytest-like summaries.

    Args:
        command: A list of strings representing the command and its arguments.
        cwd: The current working directory to execute the command in. If None,
             the current working directory of the script will be used.

    Returns:
        A tuple containing:
        - The number of failed tests (an integer). This will be sys.maxsize if
          pytest fails to run or collects no tests, indicating a regression.
        - The combined standard output and standard error of the command (a string).
    """
    try:
        logging.info(f"Running command: {' '.join(command)} in directory: {cwd if cwd else 'current'}")
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False, # Do not raise an exception for non-zero exit codes
            cwd=cwd
        )
        output = result.stdout + result.stderr
        
        failures = 0
        
        # 1. Check for specific test failures count reported by pytest
        # This regex looks for phrases like "X failed" in the pytest summary line.
        match_failures = re.search(r"===.*?(\d+)\s+failed.*?(?:,|$)", output)
        if match_failures:
            failures = int(match_failures.group(1))

        # 2. Check for no tests collected or no tests ran (pytest specific messages)
        no_tests_collected_match = re.search(r"collected 0 items", output)
        no_tests_ran_match = re.search(r"no tests ran", output)
        
        # 3. Determine if it's a regression due to non-test-failure reasons
        # If the command exited with a non-zero status code AND our regex for 'failed' tests
        # didn't find any (meaning it's an error like a syntax issue or collection error),
        # OR if pytest explicitly reported no tests were collected/ran, then it's a regression.
        if (result.returncode != 0 and failures == 0) or no_tests_collected_match or no_tests_ran_match:
            if no_tests_collected_match or no_tests_ran_match:
                logging.warning("No tests were collected or ran. This is considered a regression.")
            else:
                logging.warning(f"Command exited with non-zero status ({result.returncode}) but no 'failed' tests reported. This is considered a regression (e.g., pytest error, syntax error).")
            
            # Set failures to a very large number to ensure it's always considered a regression
            # when compared to any valid baseline_failures count.
            failures = sys.maxsize 
        
        logging.debug(f"Command output:\n{output}")
        logging.info(f"Command finished. Failures detected: {failures}")
        return failures, output
    except FileNotFoundError:
        logging.error(f"Error: Command '{command[0]}' not found. Make sure it's in your PATH.")
        sys.exit(1)
    except Exception as e:
        logging.error(f"An unexpected error occurred while running command: {e}")
        sys.exit(1)

def get_baseline_failures(test_command: list[str], cwd: Path | None) -> int:
    """
    Runs the test command once to establish a baseline for the number of failures.
    This baseline is used to detect regressions after line deletions.

    Args:
        test_command: A list of strings representing the test command.
        cwd: The current working directory for the test command.

    Returns:
        The number of failures from the initial test run.
    """
    logging.info("Establishing baseline test failures...")
    failures, _ = run_command(test_command, cwd)
    
    # If the baseline run itself indicates a regression (e.g., no tests run initially),
    # then the script cannot proceed meaningfully.
    if failures == sys.maxsize:
        logging.error("Baseline test run indicates a regression (e.g., pytest failed or no tests ran). Please fix your test setup before running the mutation tester.")
        sys.exit(1)

    logging.info(f"Baseline failures: {failures}")
    return failures

def main():
    """
    Main function to orchestrate the line deletion and testing process.
    It takes a file path, a CWD for the test command, and the test command
    as arguments, then iteratively deletes lines, runs tests, and reverts
    if regressions are found.
    """
    # Check for correct command-line arguments.
    # Expects: script.py <file_path> <cwd_for_test_command> <test_command> [args...]
    if len(sys.argv) < 4:
        # Display help message if not enough arguments are provided.
        print("Usage: python script.py <file_path> <cwd_for_test_command> <test_command> [args...]")
        print("\nArguments:")
        print("  <file_path>           The path to the source code file to be mutated.")
        print("  <cwd_for_test_command> The current working directory where the test command will be executed.")
        print("                        Use '.' for the current directory.")
        print("  <test_command> [args...] The exact command for your test suite (e.g., 'pytest test_my_module.py' or just 'pytest').")
        sys.exit(1)

    # Parse command-line arguments.
    file_path = Path(sys.argv[1])
    test_cwd = Path(sys.argv[2])
    test_command = sys.argv[3:]

    # Validate that the target file exists.
    if not file_path.is_file():
        logging.error(f"Error: File not found at '{file_path}'")
        sys.exit(1)
    
    # Validate that the test CWD exists and is a directory.
    if not test_cwd.is_dir():
        logging.error(f"Error: Current working directory for tests not found or is not a directory at '{test_cwd}'")
        sys.exit(1)


    logging.info(f"Target file: {file_path}")
    logging.info(f"Test command CWD: {test_cwd}")
    logging.info(f"Test command: {' '.join(test_command)}")

    # Record the start time of the entire script execution.
    script_start_time = time.time()

    # Read and store the original content of the file.
    # This is used as the initial "last known good content" and for full restoration if baseline fails.
    original_content = file_path.read_text().splitlines(keepends=True)
    
    # Initialize last_known_good_content with the original content.
    # This will be updated whenever a line deletion is successfully kept.
    last_known_good_content = list(original_content)

    # Get the initial number of test failures to use as a comparison baseline.
    baseline_failures = get_baseline_failures(test_command, test_cwd)

    total_successful_deletions = 0
    pass_count = 0
    exit_reason = "Completed all possible safe deletions." # Default exit reason

    try:
        # Main loop: continues until no lines can be safely deleted in a full pass.
        while True:
            pass_count += 1
            logging.info(f"\n--- Starting Pass {pass_count} ---")
            pass_start_time = time.time() # Start time for the current pass
            
            # Read the current state of the file at the beginning of each pass.
            current_lines = file_path.read_text().splitlines(keepends=True)
            if not current_lines:
                logging.info("File is empty. No more lines to process.")
                break

            successful_deletions_this_pass = 0
            lines_processed_in_pass = 0
            
            # Create a temporary mutable copy of the lines for the current pass.
            temp_lines_for_pass = list(current_lines) 
            
            # Iterate through the lines in the `temp_lines_for_pass`.
            i = 0
            while i < len(temp_lines_for_pass):
                line_to_attempt_delete = temp_lines_for_pass[i]
                
                # Create a new list of lines with the current line removed.
                test_deletion_lines = temp_lines_for_pass[:i] + temp_lines_for_pass[i+1:]
                
                # Write this modified content (with one line removed) back to the actual file.
                file_path.write_text("".join(test_deletion_lines))
                
                logging.info(f"Attempting to delete line {i+1} (originally: '{line_to_attempt_delete.strip()[:50]}...')")
                lines_processed_in_pass += 1
                
                # Run the tests on the modified file, passing the specified CWD.
                current_failures, _ = run_command(test_command, test_cwd)
                
                # Check for improvement: if current failures are strictly less than baseline.
                if current_failures < baseline_failures:
                    logging.info(f"IMPROVEMENT DETECTED! Failures decreased from {baseline_failures} to {current_failures}.")
                    logging.info("Keeping this improved version and exiting the loop.")
                    total_successful_deletions += 1 # Count this as a successful "deletion" leading to improvement
                    last_known_good_content = list(test_deletion_lines) # Update to the improved state
                    file_path.write_text("".join(last_known_good_content)) # Ensure file is written
                    exit_reason = "Improvement detected (failure rate decreased)."
                    return # Exit main function immediately
                
                # Check for regression: if current failures are less than or equal to baseline, it's safe.
                # If current_failures is sys.maxsize, this condition will be false, triggering a revert.
                elif current_failures <= baseline_failures:
                    logging.info(f"SUCCESS: Line {i+1} deleted. Failures: {current_failures} (baseline: {baseline_failures})")
                    successful_deletions_this_pass += 1
                    total_successful_deletions += 1
                    
                    # If deletion is successful, update `temp_lines_for_pass` to reflect this permanent change.
                    # The list shrinks, so the next line to check is now at the current index `i`.
                    temp_lines_for_pass = test_deletion_lines
                    # Update the last known good content to this new state
                    last_known_good_content = list(temp_lines_for_pass)
                else:
                    # If regression detected (failures increased, or pytest failed/no tests ran), revert the deletion.
                    logging.warning(f"REGRESSION: Line {i+1} caused {current_failures} failures (baseline: {baseline_failures}). Reverting.")
                    # Write the content *before* this specific deletion attempt back to the file.
                    file_path.write_text("".join(temp_lines_for_pass)) 
                    # Increment `i` to move to the next line, as this one was not deleted.
                    i += 1
                    
                time.sleep(0.1) # Small delay to prevent excessive CPU usage or file I/O contention.

            pass_end_time = time.time()
            pass_duration = pass_end_time - pass_start_time
            logging.info(f"--- Pass {pass_count} Summary: {successful_deletions_this_pass} lines successfully deleted. Duration: {pass_duration:.2f} seconds ---")

            # If no lines were successfully deleted in an entire pass, it means we can't delete any more safely.
            if successful_deletions_this_pass == 0:
                logging.info("No lines could be safely deleted in this pass. Breaking the loop.")
                break

    except KeyboardInterrupt:
        logging.info("\nScript interrupted by user.")
        exit_reason = "Script interrupted by user."
        # If interrupted, restore to the last known good state.
        logging.info("Restoring file to last known good state due to interruption...")
        file_path.write_text("".join(last_known_good_content))
        logging.info("File restored to last known good state.")
    except Exception as e:
        logging.error(f"An unhandled error occurred: {e}")
        exit_reason = f"An unhandled error occurred: {e}"
        # In case of an unexpected error, also restore to the last known good state.
        logging.info("Restoring file to last known good state due to an error...")
        file_path.write_text("".join(last_known_good_content))
        logging.info("File restored to last known good state.")
    finally:
        script_end_time = time.time()
        total_script_duration = script_end_time - script_start_time

        logging.info("\n--- Final Report ---")
        logging.info(f"Total passes completed: {pass_count}")
        logging.info(f"Total lines successfully deleted: {total_successful_deletions}")
        logging.info(f"Total script duration: {total_script_duration:.2f} seconds")
        logging.info(f"Exit Reason: {exit_reason}")
        # The file is left in its modified state if the script runs to completion without interruption.
        # If interrupted or an error occurred, it's restored to the last known good state.
        logging.info(f"File '{file_path}' has been left with all successful deletions up to the last known good state or an improved state.")

if __name__ == "__main__":
    main()

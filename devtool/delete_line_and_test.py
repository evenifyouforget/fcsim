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
from dataclasses import dataclass
from typing import List, Optional, Union # Corrected imports for Python 3.8 compatibility
import random # Added for randomizing line order

# Configure logging to show information about the process.
# This helps in tracking progress, successful/failed deletions, and any issues.
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# Define the interval for re-checking the original baseline
RECHECK_INTERVAL = 10

# Define unique exit codes for specific error conditions
EXIT_CODE_SUCCESS = 0
EXIT_CODE_INVALID_ARGS = 1
EXIT_CODE_FILE_NOT_FOUND = 2
EXIT_CODE_CWD_NOT_FOUND = 3
EXIT_CODE_COMMAND_NOT_FOUND = 4
EXIT_CODE_UNEXPECTED_COMMAND_ERROR = 5
EXIT_CODE_BASELINE_ISSUE = 6


@dataclass
class TestRunResult:
    """
    Represents the outcome of a test command execution.

    Attributes:
        failures: The number of failed tests.
        output: The full stdout and stderr from the command.
        is_successful: True if the test command ran without issues (e.g., pytest
                       collected and ran tests), False otherwise (e.g., pytest error,
                       no tests collected).
        error_message: An optional message detailing why the test run was not successful.
    """
    failures: int
    output: str
    is_successful: bool
    error_message: Optional[str] = None # Use Optional for Python 3.8

class MutationTester:
    """
    A class to perform crude mutation testing by iteratively deleting lines
    from a target file and running a test command.
    """
    def __init__(self, file_path: Path, test_cwd: Path, test_command: List[str]):
        """
        Initializes the MutationTester with file paths and test command.

        Args:
            file_path: The path to the source code file to be mutated.
            test_cwd: The current working directory where the test command will be executed.
            test_command: The exact command for the test suite.
        """
        self.file_path = file_path
        self.test_cwd = test_cwd
        self.test_command = test_command

        self.original_content: List[str] = []
        self.last_known_good_content: List[str] = []
        self.initial_baseline_failures: int = 0
        self.current_target_failures: int = 0

        self.total_successful_deletions: int = 0
        self.pass_count: int = 0
        self.edit_counter: int = 0 # Counter for successful deletions since last baseline re-check

        self.script_start_time: float = 0.0
        self.total_command_runs: int = 0
        self.total_command_execution_time: float = 0.0
        self.exit_reason: str = "Completed all possible safe deletions."

    def _run_test_command(self, command: List[str], cwd: Optional[Path] = None) -> TestRunResult:
        """
        Runs a shell command and returns a TestRunResult object.
        Updates global counters for time statistics.
        """
        command_start_time = time.time()
        try:
            logging.info(f"Running command: {' '.join(command)} in directory: {cwd if cwd else 'current'}")
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                check=False,
                cwd=cwd
            )
            output = result.stdout + result.stderr
            
            failures = 0
            is_successful = True
            error_message = None

            match_failures = re.search(r"===.*?(\d+)\s+failed.*?(?:,|$)", output)
            if match_failures:
                failures = int(match_failures.group(1))

            no_tests_collected_match = re.search(r"collected 0 items", output)
            no_tests_ran_match = re.search(r"no tests ran", output)
            
            if (result.returncode != 0 and failures == 0) or no_tests_collected_match or no_tests_ran_match:
                is_successful = False
                if no_tests_collected_match or no_tests_ran_match:
                    error_message = "No tests were collected or ran."
                    logging.warning(f"{error_message} This is considered a regression.")
                else:
                    error_message = f"Command exited with non-zero status ({result.returncode}) but no 'failed' tests reported (e.g., pytest error, syntax error)."
                    logging.warning(f"{error_message} This is considered a regression.")
                
            command_end_time = time.time()
            command_duration = command_end_time - command_start_time
            self.total_command_runs += 1
            self.total_command_execution_time += command_duration

            logging.debug(f"Command output:\n{output}")
            logging.info(f"Command finished. Failures detected: {failures}, Successful Run: {is_successful}, Duration: {command_duration:.2f}s")
            return TestRunResult(failures=failures, output=output, is_successful=is_successful, error_message=error_message)
        except FileNotFoundError:
            error_msg = f"Error: Command '{command[0]}' not found. Make sure it's in your PATH."
            logging.error(error_msg)
            sys.exit(EXIT_CODE_COMMAND_NOT_FOUND)
        except Exception as e:
            error_msg = f"An unexpected error occurred while running command: {e}"
            logging.error(error_msg)
            sys.exit(EXIT_CODE_UNEXPECTED_COMMAND_ERROR)

    def _get_baseline_failures(self) -> int:
        """
        Runs the test command once to establish a baseline for the number of failures.
        """
        logging.info("Establishing baseline test failures...")
        baseline_result = self._run_test_command(self.test_command, self.test_cwd)
        
        if not baseline_result.is_successful:
            logging.error(f"Baseline test run indicates an issue: {baseline_result.error_message}. Please fix your test setup before running the mutation tester.")
            sys.exit(EXIT_CODE_BASELINE_ISSUE)

        logging.info(f"Baseline failures: {baseline_result.failures}")
        return baseline_result.failures

    def _restore_file(self, content: List[str], reason: str = ""):
        """Restores the target file content."""
        if reason:
            logging.info(f"Restoring file to last known good state due to {reason}...")
        self.file_path.write_text("".join(content))
        logging.info("File restored to last known good state.")

    def _generate_report(self):
        """Generates the final report."""
        script_end_time = time.time()
        total_script_duration = script_end_time - self.script_start_time
        avg_time_per_run_final = self.total_command_execution_time / self.total_command_runs if self.total_command_runs > 0 else 0

        logging.info("\n--- Final Report ---")
        logging.info(f"Total passes completed: {self.pass_count}")
        logging.info(f"Total lines successfully deleted: {self.total_successful_deletions}")
        logging.info(f"Total script duration: {total_script_duration:.2f} seconds")
        logging.info(f"Total test runs performed: {self.total_command_runs}")
        logging.info(f"Average time per test run: {avg_time_per_run_final:.2f} seconds")
        logging.info(f"Exit Reason: {self.exit_reason}")
        logging.info(f"File '{self.file_path}' has been left with all successful deletions up to the last known good state or an improved state.")

    def run_mutation_process(self):
        """
        Orchestrates the line deletion and testing process.
        """
        self.script_start_time = time.time()

        # Read and store the original content of the file.
        self.original_content = self.file_path.read_text().splitlines(keepends=True)
        self.last_known_good_content = list(self.original_content) # Changed List[str](...) to list(...)

        self.initial_baseline_failures = self._get_baseline_failures()
        self.current_target_failures = self.initial_baseline_failures 

        try:
            while True:
                self.pass_count += 1
                logging.info(f"\n--- Starting Pass {self.pass_count} ---")
                pass_start_time = time.time()
                
                current_lines_at_pass_start = self.file_path.read_text().splitlines(keepends=True)
                if not current_lines_at_pass_start:
                    logging.info("File is empty. No more lines to process.")
                    break

                successful_deletions_this_pass = 0
                
                # Create a mutable list representing the file's state within this pass.
                # This list will be updated as lines are successfully deleted.
                current_file_state_for_pass = list(current_lines_at_pass_start) # Changed List[str](...) to list(...)

                # Create a list of lines to attempt deleting, in random order.
                # This ensures each line is considered once per pass, but the order is random.
                lines_to_attempt_deletion_in_random_order = list(current_lines_at_pass_start) # Changed List[str](...) to list(...)
                random.shuffle(lines_to_attempt_deletion_in_random_order)
                
                total_lines_in_pass = len(lines_to_attempt_deletion_in_random_order)
                lines_processed_in_pass_counter = 0 # Counter for progress within the current pass

                # Iterate through the lines in the randomized order
                for line_to_attempt_delete in lines_to_attempt_deletion_in_random_order:
                    lines_processed_in_pass_counter += 1 # Increment for each line attempted
                    
                    # Find the current index of this line in the mutable file state.
                    # It might not be found if it was already successfully deleted by a prior random attempt in this pass.
                    try:
                        index_in_current_state = current_file_state_for_pass.index(line_to_attempt_delete)
                    except ValueError:
                        # This line was already deleted in a previous successful attempt in this pass. Skip.
                        logging.info(f"Skipping already deleted line ({lines_processed_in_pass_counter}/{total_lines_in_pass})")
                        continue

                    # Create a new list of lines with the current line removed.
                    test_deletion_lines = current_file_state_for_pass[:index_in_current_state] + \
                                          current_file_state_for_pass[index_in_current_state+1:]
                    
                    self.file_path.write_text("".join(test_deletion_lines))
                    
                    logging.info(f"Attempting to delete line ({lines_processed_in_pass_counter}/{total_lines_in_pass}) (originally: '{line_to_attempt_delete.strip()[:50]}...')")
                    
                    current_test_run_result = self._run_test_command(self.test_command, self.test_cwd)
                    
                    is_acceptable_change = current_test_run_result.is_successful and \
                                           current_test_run_result.failures <= self.current_target_failures

                    if current_test_run_result.is_successful and current_test_run_result.failures < self.current_target_failures:
                        logging.info(f"IMPROVEMENT DETECTED! Failures decreased from {self.current_target_failures} to {current_test_run_result.failures}.")
                        logging.info("Keeping this improved version and exiting the loop.")
                        self.total_successful_deletions += 1
                        self.last_known_good_content = list(test_deletion_lines) # Changed List[str](...) to list(...)
                        self.file_path.write_text("".join(self.last_known_good_content))
                        self.exit_reason = "Improvement detected (failure rate decreased)."
                        return
                    
                    elif is_acceptable_change:
                        logging.info(f"SUCCESS: Line deleted. Failures: {current_test_run_result.failures} (baseline: {self.current_target_failures})")
                        successful_deletions_this_pass += 1
                        self.total_successful_deletions += 1
                        self.edit_counter += 1
                        
                        # Update the mutable state for this pass.
                        current_file_state_for_pass = test_deletion_lines
                        # Update the last known good content (persists across passes)
                        self.last_known_good_content = list(current_file_state_for_pass) # Changed List[str](...) to list(...)
                    else:
                        regression_details = current_test_run_result.error_message if not current_test_run_result.is_successful else \
                                             f"{current_test_run_result.failures} failures (baseline: {self.current_target_failures})"
                        logging.warning(f"REGRESSION: Line caused an issue: {regression_details}. Reverting.")
                        # Revert the file to the state *before* this specific failed deletion attempt.
                        self.file_path.write_text("".join(current_file_state_for_pass)) 
                        # No change to `current_file_state_for_pass` as this attempt failed.
                    
                    # --- Periodic Baseline Re-check ---
                    if self.edit_counter > 0 and self.edit_counter % RECHECK_INTERVAL == 0:
                        logging.info(f"--- Performing periodic baseline re-check (every {RECHECK_INTERVAL} successful edits) ---")
                        
                        state_before_recheck = list(self.file_path.read_text().splitlines(keepends=True)) # Changed List[str](...) to list(...)
                        self.file_path.write_text("".join(self.original_content))
                        
                        recheck_result = self._run_test_command(self.test_command, self.test_cwd)
                        
                        if recheck_result.is_successful and recheck_result.failures <= self.initial_baseline_failures:
                            logging.info(f"Periodic re-check PASSED! Original baseline ({self.initial_baseline_failures}) still met. Current failures: {recheck_result.failures}.")
                            self.file_path.write_text("".join(state_before_recheck))
                            self.edit_counter = 0 
                        else:
                            logging.error(f"Periodic re-check FAILED! Original baseline ({self.initial_baseline_failures}) not met. Current failures: {recheck_result.failures}. Issue: {recheck_result.error_message}")
                            logging.error("This indicates a severe regression or issue with the test environment that even the original file cannot pass.")
                            self._restore_file(self.last_known_good_content, "periodic re-check failure")
                            self.edit_counter = 0
                            # sys.exit(EXIT_CODE_BASELINE_RECHECK_FAILED) # Uncomment to force exit on critical baseline re-check failure
                    
                    time.sleep(0.1)

                pass_end_time = time.time()
                pass_duration = pass_end_time - pass_start_time
                
                elapsed_time = time.time() - self.script_start_time
                avg_time_per_run = self.total_command_execution_time / self.total_command_runs if self.total_command_runs > 0 else 0

                logging.info(f"--- Pass {self.pass_count} Summary: {successful_deletions_this_pass} lines successfully deleted. Duration: {pass_duration:.2f} seconds ---")
                logging.info(f"--- Overall Stats: Elapsed Time: {elapsed_time:.2f}s | Total Runs: {self.total_command_runs} | Avg Time/Run: {avg_time_per_run:.2f}s ---")

                if successful_deletions_this_pass == 0:
                    logging.info("No lines could be safely deleted in this pass. Breaking the loop.")
                    break

        except KeyboardInterrupt:
            logging.info("\nScript interrupted by user.")
            self.exit_reason = "Script interrupted by user."
            self._restore_file(self.last_known_good_content, "interruption")
            sys.exit(EXIT_CODE_SUCCESS)
        except Exception as e:
            logging.error(f"An unhandled error occurred: {e}")
            self.exit_reason = f"An unhandled error occurred: {e}"
            self._restore_file(self.last_known_good_content, "an unhandled error")
            sys.exit(EXIT_CODE_UNEXPECTED_COMMAND_ERROR)
        finally:
            self._generate_report()
            # If the script reaches here without an explicit sys.exit() from an error, it means it completed successfully.
            # sys.exit(EXIT_CODE_SUCCESS) # This is implicitly handled if no other sys.exit() is called.

if __name__ == "__main__":
    # Command-line argument parsing and initial validation
    if len(sys.argv) < 4:
        print("Usage: python script.py <file_path> <cwd_for_test_command> <test_command> [args...]")
        print("\nArguments:")
        print("  <file_path>           The path to the source code file to be mutated.")
        print("  <cwd_for_test_command> The current working directory where the test command will be executed.")
        print("                        Use '.' for the current directory.")
        print("  <test_command> [args...] The exact command for your test suite (e.g., 'pytest test_my_module.py' or just 'pytest').")
        sys.exit(EXIT_CODE_INVALID_ARGS)

    file_path_arg = Path(sys.argv[1])
    test_cwd_arg = Path(sys.argv[2])
    test_command_arg = sys.argv[3:]

    if not file_path_arg.is_file():
        logging.error(f"Error: File not found at '{file_path_arg}'")
        sys.exit(EXIT_CODE_FILE_NOT_FOUND)
    
    if not test_cwd_arg.is_dir():
        logging.error(f"Error: Current working directory for tests not found or is not a directory at '{test_cwd_arg}'")
        sys.exit(EXIT_CODE_CWD_NOT_FOUND)

    # Create an instance of the MutationTester and run the process
    tester = MutationTester(file_path_arg, test_cwd_arg, test_command_arg)
    tester.run_mutation_process()

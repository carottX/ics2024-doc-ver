#include <stdio.h>

class Test {
public:
  /**
   * @brief Prints a greeting message with the function name and line number.
   * 
   * This method outputs a formatted string to the standard output, displaying a greeting message
   * along with the name of the function (__func__) and the line number (__LINE__) where the message
   * is printed. The message is "Hello, Project-N!".
   */
  Test()  { printf("%s,%d: Hello, Project-N!\n", __func__, __LINE__); }
  /**
   * @brief Destructor for the Test class.
   * 
   * This method is automatically called when an instance of the Test class is destroyed.
   * It prints a farewell message to the standard output, including the name of the
   * function (__func__) and the line number (__LINE__) where the destructor is defined.
   * The message format is: "Test,<line_number>: Goodbye, Project-N!".
   */
  ~Test() { printf("%s,%d: Goodbye, Project-N!\n", __func__, __LINE__); }
};

Test test;

/**
 * @brief The main entry point of the program.
 *
 * This method prints a "Hello world!" message to the standard output, 
 * along with the name of the function (main) and the line number where 
 * the message is printed. It uses the `printf` function to format and 
 * display the message. The `__func__` macro is used to get the current 
 * function name, and the `__LINE__` macro is used to get the current 
 * line number in the source file.
 *
 * @return int Returns 0 to indicate successful program execution.
 */
int main() {
  printf("%s,%d: Hello world!\n", __func__, __LINE__);
  return 0;
}

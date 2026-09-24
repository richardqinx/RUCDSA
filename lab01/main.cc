/*
 * main.cc
 *
 * Test driver for the Triplet class.
 * Operations are read from standard input and results are written
 * to standard output.
 */

#include "Triplet.hh"

#include <iostream>
#include <string>


/*
 * Read one operation from standard input and execute it.
 *
 * Supported operations:
 *
 *   ADD    a b c  x y z
 *   SUB    a b c  x y z
 *   SCALE  a b c  k
 *   DOT    a b c  x y z
 *   MAX    a b c
 *   ASC    a b c
 *   DESC   a b c
 *
 * The result is written to standard output.
 */
int
main ()
{
  std::string op;

  if (!(std::cin >> op))
    return 1;

  if (op == "ADD")
    {
      double a, b, c;
      double x, y, z;

      if (!(std::cin >> a >> b >> c >> x >> y >> z))
        return 1;

      Triplet<double> lhs (a, b, c);
      Triplet<double> rhs (x, y, z);
      Triplet<double> result = lhs + rhs;

      std::cout << result[0] << ' '
                << result[1] << ' '
                << result[2] << '\n';
    }
  else if (op == "SUB")
    {
      double a, b, c;
      double x, y, z;

      if (!(std::cin >> a >> b >> c >> x >> y >> z))
        return 1;

      Triplet<double> lhs (a, b, c);
      Triplet<double> rhs (x, y, z);
      Triplet<double> result = lhs - rhs;

      std::cout << result[0] << ' '
                << result[1] << ' '
                << result[2] << '\n';
    }
  else if (op == "SCALE")
    {
      double a, b, c;
      double scalar;

      if (!(std::cin >> a >> b >> c >> scalar))
        return 1;

      Triplet<double> v (a, b, c);
      Triplet<double> result = v * scalar;

      std::cout << result[0] << ' '
                << result[1] << ' '
                << result[2] << '\n';
    }
  else if (op == "DOT")
    {
      double a, b, c;
      double x, y, z;

      if (!(std::cin >> a >> b >> c >> x >> y >> z))
        return 1;

      Triplet<double> lhs (a, b, c);
      Triplet<double> rhs (x, y, z);

      std::cout << lhs * rhs << '\n';
    }
  else if (op == "MAX")
    {
      double a, b, c;

      if (!(std::cin >> a >> b >> c))
        return 1;

      Triplet<double> v (a, b, c);

      std::cout << v.max () << '\n';
    }
  else if (op == "ASC")
    {
      double a, b, c;

      if (!(std::cin >> a >> b >> c))
        return 1;

      Triplet<double> v (a, b, c);

      std::cout << std::boolalpha
                << v.is_ascending ()
                << '\n';
    }
  else if (op == "DESC")
    {
      double a, b, c;

      if (!(std::cin >> a >> b >> c))
        return 1;

      Triplet<double> v (a, b, c);

      std::cout << std::boolalpha
                << v.is_descending ()
                << '\n';
    }
  else
    {
      std::cerr << "Unknown operation: " << op << '\n';
      return 1;
    }

  return 0;
}

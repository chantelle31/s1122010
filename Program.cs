using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace exerices2
{
    internal class Program
    {
        static void Main(string[] args)
        {
            double[] numbers = new double[3];
            for(int i=0;i<3;i++)
            {
                Console.WriteLine("請輸入第{i+1}個數字");
                numbers[i] = Convert.ToDouble(Console.ReadLine());
            }
            Array.Sort(numbers);
            Array.Reverse(numbers);
            Console.WriteLine("\nfinal");
            foreach (double num in numbers)
            {
                Console.WriteLine(num);
            }
               

        }
    }
}

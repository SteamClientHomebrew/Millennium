using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.NetworkInformation;
using System.Text;
using System.Threading.Tasks;
internal class Uri
{
    public static string Build(params string[] data)
    {
        string output = default;

        for (int i = 0; i < data.Length; i++)
        {
            output += data[i];

            if (i < data.Length - 1)
            {
                output += "/";
            }
        }

        return output;
    }
}

using System;
using System.Collections.Generic;
using System.Linq;

namespace millennium_silent
{
    internal class user
    {
        public static bool should_install()
        {
            return get_parameters().Where(arg => arg.Key.Equals("install")).Select(key => bool.Parse(key.Value)).FirstOrDefault();
        }

        public static Dictionary<string, string> get_parameters()
        {
            return Environment.GetCommandLineArgs()?.Where(arg => arg.Split('=').Length == 2).ToDictionary(arg => arg.Split('=')[0], arg => arg.Split('=')[1])
            ?? new Dictionary<string, string>();
        }
        public static string fetch_desired_skin()
        {
            return user.get_parameters().Where(arg => arg.Key.Equals("skin")).Select(key => key.Value).FirstOrDefault() ?? "default";
        }
    }
}

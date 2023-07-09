using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace millennium_silent
{
    internal class steam
    {
        [SuppressMessage("Interoperability", "CA1416:Validate platform compatibility", Justification = "millennium is windows only")]
        public static bool set_skin()
        {
            RegistryKey key = Registry.CurrentUser.CreateSubKey("Software\\Millennium");
            if (key != null)
            {
                key.SetValue("active-skin", user.fetch_desired_skin());
                return key.GetValue("active-skin") != null;
            }
            return false;
        }

        public static string fetch_path()
        {
            string steamPath = string.Empty;
            RegistryKey steamKey = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam");
            if (steamKey != null)
            {
                steamPath = steamKey.GetValue("SteamPath") as string;
                steamKey.Close();
            }
            return steamPath;
        }
    }
}

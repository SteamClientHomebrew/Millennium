using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net.NetworkInformation;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Millennium.Installer.Controller
{
    internal class SteamController
    {
        public SteamController() { }

        public class Steam
        {
            public static string getInstallPath()
            {
                RegistryKey key = Registry.CurrentUser.OpenSubKey("Software\\Valve\\Steam");

                if (key == null)
                {
                    MessageBox.Show("Steam not found in the Registry. Make sure Steam is installed.");
                }

                string steamPath = key?.GetValue("SteamPath") as string;
                key.Close();

                return steamPath ?? "null";
            }

            public static void forceKillSteam()
            {
                Process[] processes = Process.GetProcessesByName("Steam");

                if (processes.Length > 0)
                {
                    foreach (var process in processes)
                    {
                        try
                        {
                            process.Kill();
                            Console.WriteLine($"Killed process: {process.ProcessName} (PID: {process.Id})");
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"Failed to kill process: {process.ProcessName} (PID: {process.Id}) - {ex.Message}");
                        }
                    }
                }
                else
                {
                    Console.WriteLine("Steam isn't running, nothing to close.");
                }
            }
        }
    }
}

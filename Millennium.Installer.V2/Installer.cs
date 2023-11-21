using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Millennium.Installer.V2
{
    public partial class Installer : Form
    {
        void InitializeLoader()
        {
            Thread.Sleep(2000);

            var api = new GitHubApiClient("https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest");
            var itemQuery = api.GetDownloadListAsync().Result.ToArray();

            Controller.SteamController.Steam.forceKillSteam();

            for (int i = 0; i < itemQuery.Count(); i++)
            {
                itemQuery[i].DownloadFileSync(Controller.SteamController.Steam.getInstallPath());
                statusLabel.Text = $"Downloading [{i + 1}/{itemQuery.Count()}] files...";
                progressBar.Value = (int)(((i + 1.00) / itemQuery.Count()) * 100);

                Thread.Sleep(1000);
            }

            statusLabel.Text = "Done! Bootstrapping Steam...";

            try
            {
                Process process = new Process();

                process.StartInfo.FileName = Controller.SteamController.Steam.getInstallPath() + "/steam.exe";
                //process.StartInfo.Arguments = "-updated";
                process.Start();

                Environment.Exit(0);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"An error occurred: {ex.Message}");
            }
        }

        public Installer()
        {
            InitializeComponent();
            new Thread(() => InitializeLoader()).Start();

            CheckForIllegalCrossThreadCalls = false;
        }

        private void exitButton_Click(object sender, EventArgs e) => Environment.Exit(0);
    }
}

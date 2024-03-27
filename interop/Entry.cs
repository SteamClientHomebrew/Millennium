using Newtonsoft.Json;
using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace updater
{
    class UpdateForce
    {
        public static bool forceUpToDate = false;
    }

    [ComVisible(true)]
    public class Entry
    {
        static readonly string API_ENDPOINT = "https://millennium.web.app";
        static readonly string API_GITHUB = "https://api.github.com";

        static private string GetHash(dynamic data, string hash)
        {
            string details = default;

            try
            {
                string owner = data.owner;
                string repo = data.repo;

                UpdateForce.forceUpToDate = !HashManager.GetHashFromTheme(owner, repo, hash, out details);
            }
            catch (Exception ex)
            {
                Logger.Log(ex.ToString());  
            }

            return details;
        }

        static string compare(string args, string hash)
        {
            dynamic theme = JsonConvert.DeserializeObject(args);

            string owner = theme.owner;
            string repo = theme.repo;

            string diskHash = GetHash(theme, hash);

            return Uri.Build(API_GITHUB, "repos", owner, repo, "compare", diskHash + "..." + hash);
        }

        static void UpdateHash(string args, string hash)
        {
            dynamic theme = JsonConvert.DeserializeObject(args);

            string owner = theme.owner;
            string repo = theme.repo;

            HashManager.SetHash(owner, repo, hash);
        }

        static async Task<string> BootstrapUpdate(string args)
        {
            string apiUrl = Uri.Build(API_ENDPOINT, "api", "v2", "update");

            try
            {
                using (HttpClient httpClient = new HttpClient())
                {
                    HttpContent content = new StringContent(args, System.Text.Encoding.UTF8, "application/json");
                    HttpResponseMessage response = await httpClient.PostAsync(apiUrl, content);

                    if (response.IsSuccessStatusCode)
                    {
                        return await response.Content.ReadAsStringAsync();
                    }
                    else
                    {
                        Logger.Log($"Error: {response.StatusCode} - {response.ReasonPhrase}");
                    }
                }
            }
            catch (Exception ex)
            {
                Logger.Log(ex.ToString());
            }
            return string.Empty;
        }

        private static bool Initialize(string args)
        {
            string result = BootstrapUpdate(args).Result;

            if (string.IsNullOrEmpty(result))
            {
                Logger.Log("[-] A fatal error occured while trying to update a theme");
                return false;
            }

            dynamic data = JsonConvert.DeserializeObject(result);

            Logger.Log(data.ToString());

            string hash = data?.data?.latestHash;
            string downloadUrl = data?.data?.download;

            Logger.Log($"[+] Latest hash from github: {hash}");

            if (downloadUrl != null)
            {
                Inspector inspector = new Inspector(downloadUrl, compare(args, hash));

                if (inspector.Start())
                {
                    UpdateHash(args, hash);
                    return true;
                }

                return false;
            }

            Logger.Log("Received malformed response from updater API");
            return false;
        }

        [ComVisible(true)]
        public static int Bootstrapper(string args)
        {
            try
            {
                // Create directories recursively
                Directory.CreateDirectory(Path.GetDirectoryName(Logger.LogFilePath));
                Console.WriteLine($"Directories created successfully: {Path.GetDirectoryName(Logger.LogFilePath)}");
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error: {e.Message}");
            }

            File.WriteAllText(Logger.LogFilePath, string.Empty);

            ServicePointManager.Expect100Continue = true;
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;

            string themeName = (JsonConvert.DeserializeObject(args) as dynamic).repo;
            Logger.Log($"[Bootstrapper(updater.dll)] Starting update for {themeName}...");

            return Convert.ToInt32(Entry.Initialize(args));
        }
    }
}
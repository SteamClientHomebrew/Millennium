using Newtonsoft.Json.Linq;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Threading.Tasks;

namespace millennium_silent
{
    internal class github
    {
        private static readonly string github_api_endpoint = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";

        public static readonly HttpClient http_client = new HttpClient();

        public class release_info
        {
            public string body { get; set; }
            public string version { get; set; }
            public List<attachment> attachments { get; set; }
        }

        public class attachment
        {
            public string name { get; set; }
            public int download_count { get; set; }
            public double file_size { get; set; }
            public string download_url { get; set; }
        }

        public static async Task<Newtonsoft.Json.Linq.JObject> get_repo()
        {
            var client = new HttpClient();
            client.DefaultRequestHeaders.UserAgent.ParseAdd("millennium-installer");

            return JObject.Parse(await (await client.GetAsync(github_api_endpoint)).Content.ReadAsStringAsync());
        }

        public static async Task download(string url, string file_path)
        {
            using (var response = await github.http_client.GetAsync(url, HttpCompletionOption.ResponseHeadersRead))
            {
                response.EnsureSuccessStatusCode();

                using (var fileStream = new FileStream(file_path, FileMode.Create, FileAccess.Write, FileShare.None))
                {
                    await response.Content.CopyToAsync(fileStream);
                }
            }
        }
    }
}

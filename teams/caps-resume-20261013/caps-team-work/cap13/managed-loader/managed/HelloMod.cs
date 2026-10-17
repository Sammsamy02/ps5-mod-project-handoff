// CAP13 managed route — MINIMAL FIXTURE MOD. SOURCE STAGE ONLY.
//
// IDENTITY LAW: this is OUR fixture, authored in this worktree, compiled by
// us with the cap10 recipe. It is labeled as such everywhere and must NEVER
// be presented as a downloaded/real community mod (original-mod-identity
// law; the #13 assignment's "do not call that a real-mod pass" rule).
// Whether any real mscorlib-only mod exists in the wild is an explicit
// open question in DESIGN/managed-dll-route.md section 8.
//
// References: mscorlib + Mbm.ModSdk ONLY (admitted closure, section 2c).
// Intended visible action: logs a hello marker at load, reads one engine
// object/property once (exercise the #9-bound engine surface), counts 64
// ticks, then quiesces itself via Host.Stop().
using Mbm.ModSdk;

namespace Mbm.Fixtures
{
    [MbmMod("mbm.fixture.hello", "CAP13 HelloMod", "1.0.0")]
    public sealed class HelloMod : MbmMod
    {
        private const int TicksToObserve = 64;
        private const string ProbeObjectPath = "/Engine/GameState"; // mock on Mac; #9-bound on target
        private const string ProbeProperty = "TickRate";

        private int _ticks;
        private bool _probeDone;

        public override void OnLoad(ModHost host)
        {
            host.Log(1 /*INFO*/, "hello: loaded, id=mbm.fixture.hello");
        }

        public override void OnTick()
        {
            _ticks++;

            if (!_probeDone)
            {
                _probeDone = true;
                ModHost.EngineObject probe = Host.FindObject(ProbeObjectPath);
                if (probe == null)
                {
                    Host.Log(2 /*WARN*/, "hello: probe object not found (expected on unbound surface)");
                }
                else
                {
                    double value;
                    if (probe.ReadNumber(ProbeProperty, out value))
                        Host.Log(1 /*INFO*/, "hello: " + ProbeObjectPath + "." + ProbeProperty +
                            " = " + value);
                    else
                        Host.Log(2 /*WARN*/, "hello: probe property unreadable");
                    probe.Release();
                }
            }

            if (_ticks == TicksToObserve)
            {
                Host.Log(1 /*INFO*/, "hello: observed " + _ticks + " ticks, requesting stop");
                Host.Stop();
            }
        }

        public override void OnUnload()
        {
            Host.Log(1 /*INFO*/, "hello: unloaded after " + _ticks + " ticks (quiesced)");
        }
    }
}

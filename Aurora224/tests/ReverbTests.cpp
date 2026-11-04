#include "Reverb.h"
#include <iostream>
#include <stdexcept>
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    for (double sr : {44100., 48000., 96000.}) for (int mode = 0; mode < 3; ++mode) {
        aurora::Reverb reverb; auto p = aurora::defaults; p[0] = mode / 2.; p[9] = 1.;
        reverb.prepare(sr, p);
        double early = 0., late = 0., stereo = 0.;
        for (int i = 0; i < int(sr * 10); ++i) {
            float l, r; reverb.tick(i == 0 ? 1.f : 0.f, 0.f, l, r, p);
            require(std::isfinite(l) && std::isfinite(r), "nonfinite output");
            require(std::abs(l) < 4.f && std::abs(r) < 4.f, "unbounded impulse response");
            double e = l*l + r*r;
            if (i < sr * 2) early += e;
            if (i > sr * 8) late += e;
            stereo += (l-r)*(l-r);
        }
        require(early > 1.e-5, "missing reverb"); require(stereo > 1.e-5, "missing stereo decorrelation");
        require(late < early * .001, "tail does not decay");
    }
    aurora::Reverb dry; auto p = aurora::defaults; p[9] = 0.; dry.prepare(48000, p);
    float l, r; dry.tick(.37f, -.25f, l, r, p);
    require(l == .37f && r == -.25f, "dry mix alters signal");
    for (int mode = 0; mode < 3; ++mode) {
        p.fill(1.); p[0] = mode * .5; dry.prepare(48000, p);
        for (int i = 0; i < 480000; ++i) {
            if (i % 12000 == 0) p[3] = p[3] > .5 ? 0. : 1.;
            dry.tick(i < 48000 ? .2f : 0.f, i < 48000 ? -.1f : 0.f, l, r, p);
            require(std::isfinite(l) && std::isfinite(r) && std::abs(l) < 8 && std::abs(r) < 8, "unstable extreme settings");
        }
    }
    std::cout << "PASS: 3 sample rates, 3 algorithms, stereo, decay, dry mix, extreme automation\n";
}

///Compare tracking efficiency, fake rate, multiple-reconstruction and secondary rates between two
///samples that contain the SAME events (e.g. the same GEN-SIM/DIGI reconstructed with two releases).
///
///Inputs: two output files of
///  cmsRun run_PbPb_cfg.py sample="MC_RecoDebug" ...
///with the per-event tree HITrackCorrections/evtTree (fillEventTree = True, the default in run_PbPb_cfg.py).
///
///Run:
///  root -l -b -q 'plotHist_CompareTwoSamples.C("trk_rereco.root","trk_prompt.root","ReReco","Prompt")'
///Arguments: file1, file2, label1, label2, [cent_min, cent_max in %, default 0-100], [output directory]
///
///Output, one row of four panels per rate (ready for slides):
///  Efficiency, FakeRate, MultipleReco, Secondary  .pdf/.png
///  panels: vs pT for |eta| < 1 and |eta| < 2.4, vs eta for pT > 0.4 and pT > 2 GeV (bin edges, see below)
///and all histograms in compareTwoSamples.root. Ratio panels are sample1 / sample2.
///
///Uncertainties (events are the independent units; events are matched by run, lumi, event):
///  rate r = N/D summed over events.
///  single sample : Var(r) = r^2 * n/(n-1) * sum_e (N_e/N - D_e/D)^2
///  ratio R=r1/r2  : Var(R) = R^2 * n/(n-1) * sum_e (N1_e/N1 - D1_e/D1 - N2_e/N2 + D2_e/D2)^2
///The ratio term uses the event pairing, so fluctuations common to both samples (same particles,
///mostly the same tracks) cancel and only genuine reconstruction differences remain.
///The macro prints the median (paired / unpaired) ratio uncertainty for each rate.

#include "TFile.h"
#include "TTree.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"
#include "TTreeReaderArray.h"
#include "TH1D.h"
#include "TH2.h"
#include "TCanvas.h"
#include "TPad.h"
#include "TLegend.h"
#include "TLine.h"
#include "TLatex.h"
#include "TStyle.h"
#include "TSystem.h"
#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

///*** Important parameters
//projection ranges, same as plotHist2D.C
//NOTE: with the default etaBins (... -1.2, -0.8, ... 0.8, 1.2 ...) "|eta| < 1" really is |eta| < 1.2
double etaNarrow = 0.99;   // pT projections: |eta| < etaNarrow (bin edges)
double etaWide   = 2.39;   // pT projections: |eta| < etaWide   (bin edges)
double ptLow     = 0.41;   // eta projections: pT > ptLow  (bin edges)
double ptHigh    = 2.01;   // eta projections: pT > ptHigh (bin edges)
double ptPlotMin = 0.3;    // x-range for pT plots
double ptPlotMax = 100.;
double ratioHalfRange = -1; // ratio panel shown as 1 +- this; <= 0 means automatic

const char* dirName = "HITrackCorrections";

///Auxiliary types and functions

typedef std::array<unsigned long long,3> EvtKey; // run, lumi, event

const std::vector<std::string> histNames = {"hsim","heff","hmul","hrec","hfak","hsec"};

struct Projection { const char* name; bool vsPt; double cut; const char* text; };
const std::vector<Projection> projections = {
  {"vsPt_etaNarrow", true,  etaNarrow, "|#eta| < 1.2"},
  {"vsPt_etaWide",   true,  etaWide,   "|#eta| < 2.4"},
  {"vsEta_ptLow",    false, ptLow,     "p_{T} > 0.4 GeV"},
  {"vsEta_ptHigh",   false, ptHigh,    "p_{T} > 2 GeV"},
};

struct Rate { const char* num; const char* den; const char* title; const char* fig; };
const std::vector<Rate> rates = {
  {"heff", "hsim", "Absolute efficiency",                 "Efficiency"},
  {"hfak", "hrec", "Fake rate",                           "FakeRate"},
  {"hmul", "hsim", "Multiple reconstruction fraction",    "MultipleReco"},
  {"hsec", "hrec", "Non-primary reconstruction fraction", "Secondary"},
};

//Binning: global TH2 bins (eta = x, pt = y) -> 1D projection bins
struct Binning {
  int nx = 0, ny = 0;
  TH1D* templ[4] = {nullptr, nullptr, nullptr, nullptr}; // empty 1D histogram per projection
  int lo[4], hi[4];                                       // integration range on the other axis
  int offset[4]; int nTot = 0;                            // layout of the per-event projected vector
};

Binning makeBinning(TH2* h)
{
  Binning B;
  B.nx = h->GetNbinsX(); B.ny = h->GetNbinsY();
  for (int p = 0; p < 4; p++) {
    const Projection& P = projections[p];
    if (P.vsPt) {
      B.templ[p] = h->ProjectionY(Form("templ_%s", P.name), 1, 1); // pT axis
      B.lo[p] = h->GetXaxis()->FindBin(-P.cut); B.hi[p] = h->GetXaxis()->FindBin(P.cut);
    } else {
      B.templ[p] = h->ProjectionX(Form("templ_%s", P.name), 1, 1); // eta axis
      B.lo[p] = h->GetYaxis()->FindBin(P.cut);  B.hi[p] = B.ny;
    }
    B.templ[p]->Reset(); B.templ[p]->SetDirectory(0);
    B.offset[p] = B.nTot; B.nTot += B.templ[p]->GetNbinsX() + 2;
  }
  return B;
}

//Per-event data of one sample: projected counts of the 6 histograms (concatenated), centrality pass flag
struct EvtData { bool passCent = false; std::vector<float> c; };

//Read the per-event tree; project each event's (eta,pt) counts onto the 4 projections
bool readSample(const TString& fileName, const Binning& B, int cent_min, int cent_max, std::map<EvtKey, EvtData>& events)
{
  TFile* f = TFile::Open(fileName);
  if (!f || f->IsZombie()) { std::cerr << "ERROR: cannot open " << fileName << std::endl; return false; }
  if (!f->Get(Form("%s/evtTree", dirName))) {
    std::cerr << "ERROR: " << dirName << "/evtTree not found in " << fileName
              << ". Rerun cmsRun with the current run_PbPb_cfg.py (fillEventTree = True)." << std::endl;
    return false;
  }
  TTreeReader r(Form("%s/evtTree", dirName), f);
  TTreeReaderValue<UInt_t> run(r, "run"), lumi(r, "lumi");
  TTreeReaderValue<ULong64_t> evt(r, "event");
  TTreeReaderValue<Int_t> cbin(r, "cbin");
  std::vector<TTreeReaderArray<Float_t>*> arr;
  for (auto& n : histNames) arr.push_back(new TTreeReaderArray<Float_t>(r, n.c_str()));

  const int nG = (B.nx + 2) * (B.ny + 2);
  const int stride = B.nTot;
  int nDup = 0;
  bool sizeChecked = false;
  while (r.Next()) {
    if (!sizeChecked) {
      if ((int) arr[0]->GetSize() != nG) {
        std::cerr << "ERROR: evtTree array size " << arr[0]->GetSize() << " does not match the histogram binning " << nG << std::endl;
        return false;
      }
      sizeChecked = true;
    }
    EvtKey key = {*run, *lumi, *evt};
    if (events.count(key)) { nDup++; continue; }
    EvtData& ed = events[key];
    ed.passCent = (*cbin >= 2 * cent_min && *cbin < 2 * cent_max);
    ed.c.assign(histNames.size() * stride, 0.f);
    if (!ed.passCent) continue;
    for (size_t h = 0; h < histNames.size(); h++) {
      TTreeReaderArray<Float_t>& a = *arr[h];
      for (int g = 0; g < nG; g++) {
        float v = a[g];
        if (v == 0.f) continue;
        int ix = g % (B.nx + 2), iy = g / (B.nx + 2);
        for (int p = 0; p < 4; p++) {
          if (projections[p].vsPt) { if (ix >= B.lo[p] && ix <= B.hi[p]) ed.c[h * stride + B.offset[p] + iy] += v; }
          else                     { if (iy >= B.lo[p] && iy <= B.hi[p]) ed.c[h * stride + B.offset[p] + ix] += v; }
        }
      }
    }
  }
  if (nDup) std::cout << "WARNING: " << nDup << " duplicated events in " << fileName << " were skipped" << std::endl;
  for (auto a : arr) delete a;
  f->Close();
  return true;
}

double median(std::vector<double> v) { if (v.empty()) return -1; std::sort(v.begin(), v.end()); return v[v.size() / 2]; }

//Draw one panel (rates + ratio) in the current pad
void drawPanel(TH1D* h1, TH1D* h2, TH1D* r, const Projection& P, const TString& ytitle,
               const TString& label1, const TString& label2, const TString& centText, bool isEff)
{
  TVirtualPad* parent = gPad;
  TString id = Form("%s_%s_%p", h1->GetName(), P.name, (void*) parent);
  TPad* pTop = new TPad("top_" + id, "", 0, 0.30, 1, 1);
  TPad* pBot = new TPad("bot_" + id, "", 0, 0, 1, 0.30);
  pTop->SetBottomMargin(0.0); pTop->SetLeftMargin(0.17); pTop->SetRightMargin(0.04); pTop->SetTopMargin(0.06);
  pBot->SetTopMargin(0.0);    pBot->SetLeftMargin(0.17); pBot->SetRightMargin(0.04); pBot->SetBottomMargin(0.38);
  pTop->SetTicks(1, 1); pBot->SetTicks(1, 1);
  if (P.vsPt) { pTop->SetLogx(); pBot->SetLogx(); }
  pTop->Draw(); pBot->Draw();

  pTop->cd();
  h1->SetMarkerStyle(24); h1->SetMarkerColor(kBlue); h1->SetLineColor(kBlue); h1->SetMarkerSize(0.9);
  h2->SetMarkerStyle(25); h2->SetMarkerColor(kRed);  h2->SetLineColor(kRed);  h2->SetMarkerSize(0.9);
  double xlo = P.vsPt ? ptPlotMin : h1->GetXaxis()->GetXmin();
  double xhi = P.vsPt ? ptPlotMax : h1->GetXaxis()->GetXmax();
  double ymax = 0;
  for (TH1D* h : {h1, h2})
    for (int b = 1; b <= h->GetNbinsX(); b++) {
      double x = h->GetBinCenter(b);
      if (x < xlo || x > xhi) continue;
      ymax = std::max(ymax, h->GetBinContent(b) + h->GetBinError(b));
    }
  if (isEff) ymax = std::max(ymax, 1.0);
  h1->SetTitle("");
  h1->GetXaxis()->SetRangeUser(xlo, xhi);
  h1->GetYaxis()->SetRangeUser(0.0, ymax > 0 ? 1.35 * ymax : 1.0);
  h1->GetYaxis()->SetTitle(ytitle);
  h1->GetYaxis()->SetTitleSize(0.06); h1->GetYaxis()->SetTitleOffset(1.3); h1->GetYaxis()->SetLabelSize(0.055);
  h1->GetXaxis()->SetLabelSize(0);
  h1->Draw("E1");
  h2->Draw("E1 same");
  h1->Draw("E1 same");

  TLatex tl; tl.SetNDC(); tl.SetTextFont(42); tl.SetTextSize(0.05);
  tl.DrawLatex(0.21, 0.86, P.text);
  tl.DrawLatex(0.21, 0.79, centText);
  TLegend* leg = new TLegend(0.55, 0.76, 0.94, 0.90);
  leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextFont(42); leg->SetTextSize(0.05);
  leg->AddEntry(h1, label1, "lep");
  leg->AddEntry(h2, label2, "lep");
  leg->Draw();

  pBot->cd();
  double half = ratioHalfRange;
  if (half <= 0) { // automatic: cover the points with their errors, symmetric around 1
    half = 0.02;
    for (int b = 1; b <= r->GetNbinsX(); b++) {
      double x = r->GetBinCenter(b);
      if (x < xlo || x > xhi || r->GetBinContent(b) < 0) continue;
      half = std::max(half, std::fabs(r->GetBinContent(b) - 1) + r->GetBinError(b));
    }
    half = std::min(1.0, 1.15 * half);
  }
  r->SetMarkerStyle(24); r->SetMarkerColor(kBlue); r->SetLineColor(kBlue);
  r->GetXaxis()->SetRangeUser(xlo, xhi);
  r->GetYaxis()->SetRangeUser(1 - half, 1 + half);
  r->GetYaxis()->SetTitle("Ratio");
  r->GetYaxis()->SetNdivisions(504);
  r->GetYaxis()->SetTitleSize(0.13); r->GetYaxis()->SetTitleOffset(0.6); r->GetYaxis()->SetLabelSize(0.12);
  r->GetXaxis()->SetTitle(P.vsPt ? "p_{T} (GeV)" : "#eta");
  r->GetXaxis()->SetTitleSize(0.15); r->GetXaxis()->SetTitleOffset(1.0); r->GetXaxis()->SetLabelSize(0.12);
  r->GetXaxis()->SetLabelOffset(0.02);
  if (P.vsPt) r->GetXaxis()->SetMoreLogLabels();
  r->Draw("E1");
  TLine* line = new TLine(xlo, 1.0, xhi, 1.0);
  line->SetLineStyle(2); line->Draw();

  parent->cd();
}

void plotHist_CompareTwoSamples(TString fileName1 = "trk1.root", TString fileName2 = "trk2.root",
                                TString label1 = "Sample 1", TString label2 = "Sample 2",
                                int cent_min = 0, int cent_max = 100, TString outDir = "")
{
  gStyle->SetOptStat(0);
  gStyle->SetErrorX(0);
  if (outDir == "") outDir = Form("CompareTwoSamples_cent%dto%d", cent_min, cent_max);
  gSystem->Exec("mkdir -p " + outDir);
  TString centText = Form("Cent: %d-%d%%", cent_min, cent_max);

  // binning from the histograms of the first file
  TFile* fb = TFile::Open(fileName1);
  if (!fb || fb->IsZombie() || !fb->Get(Form("%s/hsim", dirName))) { std::cerr << "ERROR: cannot read " << dirName << "/hsim from " << fileName1 << std::endl; return; }
  Binning B = makeBinning((TH2*) fb->Get(Form("%s/hsim", dirName)));
  fb->Close();
  const int stride = B.nTot;
  auto hIdx = [](const char* n) { return (int) (std::find(histNames.begin(), histNames.end(), std::string(n)) - histNames.begin()); };

  std::map<EvtKey, EvtData> ev1, ev2;
  std::cout << "Reading " << fileName1 << " ..." << std::endl;
  if (!readSample(fileName1, B, cent_min, cent_max, ev1)) return;
  std::cout << "Reading " << fileName2 << " ..." << std::endl;
  if (!readSample(fileName2, B, cent_min, cent_max, ev2)) return;

  // ---- event bookkeeping
  std::set<EvtKey> keys;
  for (auto& e : ev1) keys.insert(e.first);
  for (auto& e : ev2) keys.insert(e.first);
  long nCommon = 0, nPass1 = 0, nPass2 = 0, nPassBoth = 0, nSimDiff = 0;
  const int iSim = hIdx("hsim");
  for (auto& k : keys) {
    auto i1 = ev1.find(k), i2 = ev2.find(k);
    bool in1 = i1 != ev1.end(), in2 = i2 != ev2.end();
    bool p1 = in1 && i1->second.passCent, p2 = in2 && i2->second.passCent;
    nCommon += (in1 && in2); nPass1 += p1; nPass2 += p2; nPassBoth += (p1 && p2);
    if (p1 && p2)
      for (int j = 0; j < stride; j++)
        if (i1->second.c[iSim * stride + j] != i2->second.c[iSim * stride + j]) { nSimDiff++; break; }
  }
  std::cout << "Events: " << ev1.size() << " (" << label1 << "), " << ev2.size() << " (" << label2 << "), " << nCommon << " in both" << std::endl;
  std::cout << "In " << centText << ": " << nPass1 << " (" << label1 << "), " << nPass2 << " (" << label2 << "), " << nPassBoth << " in both" << std::endl;
  if ((long) ev1.size() != nCommon || (long) ev2.size() != nCommon)
    std::cout << "WARNING: the files do not contain the same events; unmatched events count as zero in the other sample."
              << " Use eventList in cmsRun to process common events only." << std::endl;
  if (nPass1 != nPassBoth || nPass2 != nPassBoth)
    std::cout << "NOTE: some events are in this centrality range in only one sample (centrality is reconstructed"
              << " separately in each release); this is included in the ratios and their uncertainties." << std::endl;
  if (nSimDiff == 0) std::cout << "Simulated particles (hsim) identical event by event in the common events." << std::endl;
  else std::cout << "WARNING: hsim differs in " << nSimDiff << " common events: the TrackingParticles are not identical"
                 << " (different DIGI/mixing?), so the efficiency denominators differ." << std::endl;
  if (nPass1 == 0 || nPass2 == 0) { std::cerr << "ERROR: no events in this centrality range" << std::endl; return; }

  const std::vector<float> zeros(histNames.size() * stride, 0.f);
  const double nEv = keys.size(), fN = nEv > 1 ? nEv / (nEv - 1) : 1.;
  TFile fout(outDir + "/compareTwoSamples.root", "RECREATE");
  std::cout << "\nRatio uncertainty, median over filled bins: paired / unpaired" << std::endl;

  for (auto& R : rates) {
    const int iN = hIdx(R.num), iD = hIdx(R.den);
    TCanvas* c = new TCanvas(Form("c_%s", R.fig), "", 2400, 700);
    c->Divide(4, 1, 0.001, 0.001);
    std::vector<double> errRatio;

    for (int p = 0; p < 4; p++) {
      const Projection& P = projections[p];
      const int nb = B.templ[p]->GetNbinsX(), off = B.offset[p];
      std::vector<double> N1(nb + 2, 0.), D1(nb + 2, 0.), N2(nb + 2, 0.), D2(nb + 2, 0.);
      for (auto& e : ev1) for (int b = 1; b <= nb; b++) { N1[b] += e.second.c[iN * stride + off + b]; D1[b] += e.second.c[iD * stride + off + b]; }
      for (auto& e : ev2) for (int b = 1; b <= nb; b++) { N2[b] += e.second.c[iN * stride + off + b]; D2[b] += e.second.c[iD * stride + off + b]; }

      // sums of squared influence terms: single samples (z1, z2) and paired ratio (zr)
      std::vector<double> Z1(nb + 2, 0.), Z2(nb + 2, 0.), ZR(nb + 2, 0.);
      for (auto& k : keys) {
        auto i1 = ev1.find(k), i2 = ev2.find(k);
        const std::vector<float>& c1 = (i1 != ev1.end()) ? i1->second.c : zeros;
        const std::vector<float>& c2 = (i2 != ev2.end()) ? i2->second.c : zeros;
        for (int b = 1; b <= nb; b++) {
          double z1 = 0, z2 = 0;
          if (N1[b] > 0 && D1[b] > 0) z1 = c1[iN * stride + off + b] / N1[b] - c1[iD * stride + off + b] / D1[b];
          if (N2[b] > 0 && D2[b] > 0) z2 = c2[iN * stride + off + b] / N2[b] - c2[iD * stride + off + b] / D2[b];
          if (i1 != ev1.end()) Z1[b] += z1 * z1;
          if (i2 != ev2.end()) Z2[b] += z2 * z2;
          ZR[b] += (z1 - z2) * (z1 - z2);
        }
      }

      TH1D* h1 = (TH1D*) B.templ[p]->Clone(Form("%s_%s_s1", R.fig, P.name));
      TH1D* h2 = (TH1D*) B.templ[p]->Clone(Form("%s_%s_s2", R.fig, P.name));
      TH1D* hr = (TH1D*) B.templ[p]->Clone(Form("%s_%s_ratio", R.fig, P.name));
      for (TH1D* h : {h1, h2, hr}) { h->SetDirectory(0); h->Reset(); }
      const double n1 = ev1.size(), n2 = ev2.size();
      for (int b = 1; b <= nb; b++) {
        double r1 = D1[b] > 0 ? N1[b] / D1[b] : 0., r2 = D2[b] > 0 ? N2[b] / D2[b] : 0.;
        // single-sample errors; a bin with N = 0 gets 1/D as a rough scale
        double e1 = N1[b] > 0 ? r1 * std::sqrt(Z1[b] * (n1 > 1 ? n1 / (n1 - 1) : 1.)) : (D1[b] > 0 ? 1. / D1[b] : 0.);
        double e2 = N2[b] > 0 ? r2 * std::sqrt(Z2[b] * (n2 > 1 ? n2 / (n2 - 1) : 1.)) : (D2[b] > 0 ? 1. / D2[b] : 0.);
        h1->SetBinContent(b, r1); h1->SetBinError(b, e1);
        h2->SetBinContent(b, r2); h2->SetBinError(b, e2);
        if (N1[b] <= 0 || N2[b] <= 0 || D1[b] <= 0 || D2[b] <= 0) { hr->SetBinContent(b, -10); hr->SetBinError(b, 0); continue; } // hidden
        double rr = r1 / r2;
        double eCorr = rr * std::sqrt(ZR[b] * fN);
        double eUnc  = rr * std::sqrt(std::pow(e1 / r1, 2) + std::pow(e2 / r2, 2));
        hr->SetBinContent(b, rr); hr->SetBinError(b, eCorr);
        if (eUnc > 0) errRatio.push_back(eCorr / eUnc);
      }
      fout.cd(); h1->Write(); h2->Write(); hr->Write();
      c->cd(p + 1);
      drawPanel(h1, h2, hr, P, R.title, label1, label2, centText, TString(R.num) == "heff");
    }
    printf("  %-14s %.3f\n", R.fig, median(errRatio));
    c->Print(outDir + "/" + R.fig + ".pdf");
    c->Print(outDir + "/" + R.fig + ".png");
  }
  fout.Close();
  std::cout << "Plots saved in " << outDir << "/" << std::endl;
}

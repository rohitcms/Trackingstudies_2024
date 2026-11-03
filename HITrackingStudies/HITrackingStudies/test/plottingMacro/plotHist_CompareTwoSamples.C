///Compare tracking efficiency, fake rate, multiple-reconstruction and secondary rates
///between two MC RECODEBUG samples (e.g. two reconstruction versions of the same MC).
///
///Inputs: two output files of
///  cmsRun run_PbPb_cfg.py sample="MC_RecoDebug" ...
///(they must contain the "HITrackCorrections" directory).
///
///Run it by doing:
///  root -l -b -q plotHist_CompareTwoSamples.C
///or, without editing the file:
///  root -l -b -q 'plotHist_CompareTwoSamples.C("trk_A.root","trk_B.root","Sample A","Sample B")'
///
///Output: PDFs in "CompareTwoSamples/" and all plotted histograms in
///"CompareTwoSamples/compareTwoSamples.root". Ratio panels are sample1 / sample2.

#include "TFile.h"
#include "TH1D.h"
#include "TH2.h"
#include "TH3.h"
#include "TCanvas.h"
#include "TPad.h"
#include "TLegend.h"
#include "TLine.h"
#include "TLatex.h"
#include "TStyle.h"
#include "TSystem.h"
#include <iostream>
#include <vector>

///*** Important parameters
//centrality selection (0-100 %). Uses the 3D histograms (eta, pT, hiBin = 2*centrality%).
//Set useCentrality = false to use the 2D histograms (all centralities).
bool useCentrality = true;
int cent_min = 0;
int cent_max = 100;

//projection ranges, same as plotHist2D.C
//NOTE: with the default etaBins (... -1.2, -0.8, ... 0.8, 1.2 ...) "|eta| < 1" really is |eta| < 1.2
double etaNarrow = 0.99;   // pT projections: |eta| < etaNarrow (bin edges)
double etaWide   = 2.39;   // pT projections: |eta| < etaWide   (bin edges)
double ptLow     = 0.41;   // eta projections: pT > ptLow  (bin edges)
double ptHigh    = 2.01;   // eta projections: pT > ptHigh (bin edges)
double ptPlotMin = 0.3;    // x-range for pT plots
double ptPlotMax = 100.;

const char* dirName = "HITrackCorrections";
const char* outDir  = "CompareTwoSamples";

///Auxiliary functions

//Get the (eta, pT) 2D histogram, either directly or projected from the 3D one in a centrality range
TH2* getHist2D(TFile* f, const TString& name, const TString& tag)
{
  TH2* h2 = nullptr;
  if (useCentrality) {
    TH3* h3 = (TH3*) f->Get(Form("%s/%s3D", dirName, name.Data()));
    if (!h3) { std::cerr << "ERROR: " << dirName << "/" << name << "3D not found in " << f->GetName() << std::endl; return nullptr; }
    int zlo = h3->GetZaxis()->FindBin(2.0*cent_min + 0.001);
    int zhi = h3->GetZaxis()->FindBin(2.0*cent_max - 0.001);
    h3->GetZaxis()->SetRange(zlo, zhi);
    TH2* p = (TH2*) h3->Project3D("yxe"); // x = eta, y = pT
    h2 = (TH2*) p->Clone(name + "_" + tag);
    delete p;
  } else {
    TH2* h = (TH2*) f->Get(Form("%s/%s", dirName, name.Data()));
    if (!h) { std::cerr << "ERROR: " << dirName << "/" << name << " not found in " << f->GetName() << std::endl; return nullptr; }
    h2 = (TH2*) h->Clone(name + "_" + tag);
  }
  h2->SetDirectory(0);
  if (h2->GetSumw2N() == 0) h2->Sumw2();
  return h2;
}

//Project numerator and denominator and return the binomial ratio num/den
TH1D* makeRate(TH2* num, TH2* den, bool vsPt, double lo, double hi, const TString& name)
{
  TH1D *n, *d;
  if (vsPt) {
    int b1 = den->GetXaxis()->FindBin(-hi);
    int b2 = den->GetXaxis()->FindBin(hi);
    n = num->ProjectionY(name + "_num", b1, b2, "e");
    d = den->ProjectionY(name + "_den", b1, b2, "e");
  } else {
    int b1 = den->GetYaxis()->FindBin(lo);
    int b2 = den->GetYaxis()->GetNbins();
    n = num->ProjectionX(name + "_num", b1, b2, "e");
    d = den->ProjectionX(name + "_den", b1, b2, "e");
  }
  TH1D* r = (TH1D*) n->Clone(name);
  r->SetDirectory(0);
  r->Divide(n, d, 1, 1, "B");
  delete n; delete d;
  return r;
}

void styleHist(TH1D* h, int color, int marker)
{
  h->SetLineColor(color); h->SetMarkerColor(color);
  h->SetMarkerStyle(marker); h->SetMarkerSize(0.9); h->SetLineWidth(2);
  h->SetStats(0);
}

//Overlay of two rates with a ratio panel
void drawComparison(TH1D* h1, TH1D* h2, const TString& ytitle, double ymin, double ymax,
                    bool vsPt, const TString& label1, const TString& label2,
                    const TString& selText, const TString& figName, TFile* fout)
{
  styleHist(h1, kRed+1, 20);
  styleHist(h2, kBlue+1, 24);

  TCanvas* c = new TCanvas("c_" + figName, "", 700, 700);
  TPad* p1 = new TPad("p1_" + figName, "", 0, 0.3, 1, 1);
  TPad* p2 = new TPad("p2_" + figName, "", 0, 0, 1, 0.3);
  p1->SetBottomMargin(0.02); p1->SetLeftMargin(0.13); p1->SetTicks();
  p2->SetTopMargin(0.02); p2->SetBottomMargin(0.32); p2->SetLeftMargin(0.13); p2->SetTicks();
  if (vsPt) { p1->SetLogx(); p2->SetLogx(); }
  p1->Draw(); p2->Draw();

  p1->cd();
  if (vsPt) h1->GetXaxis()->SetRangeUser(ptPlotMin, ptPlotMax);
  h1->SetTitle("");
  h1->GetYaxis()->SetTitle(ytitle);
  h1->GetYaxis()->SetRangeUser(ymin, ymax);
  h1->GetYaxis()->SetTitleSize(0.05); h1->GetYaxis()->SetLabelSize(0.045);
  h1->GetXaxis()->SetLabelSize(0);
  h1->Draw("E1");
  h2->Draw("E1 same");

  TLegend* leg = new TLegend(0.55, 0.72, 0.88, 0.88);
  leg->SetBorderSize(0); leg->SetFillStyle(0);
  leg->AddEntry(h1, label1, "lp");
  leg->AddEntry(h2, label2, "lp");
  leg->Draw();

  TLatex tl; tl.SetNDC(); tl.SetTextSize(0.035);
  tl.DrawLatex(0.16, 0.84, selText);
  if (useCentrality) tl.DrawLatex(0.16, 0.79, Form("Centrality %d-%d%%", cent_min, cent_max));

  p2->cd();
  TH1D* r = (TH1D*) h1->Clone("ratio_" + figName);
  r->Divide(h2);
  r->GetYaxis()->SetTitle("Ratio");
  r->GetYaxis()->SetRangeUser(0.8, 1.2);
  r->GetYaxis()->SetNdivisions(505);
  r->GetYaxis()->SetTitleSize(0.11); r->GetYaxis()->SetTitleOffset(0.5); r->GetYaxis()->SetLabelSize(0.10);
  r->GetXaxis()->SetTitle(vsPt ? "p_{T} [GeV]" : "#eta");
  r->GetXaxis()->SetTitleSize(0.12); r->GetXaxis()->SetLabelSize(0.10);
  r->SetLineColor(kBlack); r->SetMarkerColor(kBlack); r->SetMarkerStyle(20);
  r->Draw("E1");
  double xlo = vsPt ? ptPlotMin : r->GetXaxis()->GetXmin();
  double xhi = vsPt ? ptPlotMax : r->GetXaxis()->GetXmax();
  TLine* line = new TLine(xlo, 1.0, xhi, 1.0);
  line->SetLineStyle(2); line->Draw();

  c->Print(Form("%s/%s.pdf", outDir, figName.Data()));

  fout->cd();
  h1->Write(); h2->Write(); r->Write();
}

void plotHist_CompareTwoSamples(TString fileName1 = "trk1.root", TString fileName2 = "trk2.root",
                                TString label1 = "Sample 1", TString label2 = "Sample 2")
{
  gStyle->SetOptStat(0);
  gSystem->Exec(Form("mkdir -p %s", outDir));

  TFile* f1 = TFile::Open(fileName1);
  TFile* f2 = TFile::Open(fileName2);
  if (!f1 || f1->IsZombie() || !f2 || f2->IsZombie()) {
    std::cerr << "ERROR: cannot open input files" << std::endl; return;
  }

  // numerator / denominator for each rate
  struct Rate { TString num, den, title, fig; double ymin, ymax; };
  std::vector<Rate> rates = {
    {"heff", "hsim", "Absolute efficiency",               "Efficiency",      0.0, 1.1 },
    {"hfak", "hrec", "Fake rate",                         "FakeRate",        0.0, 0.3 },
    {"hmul", "hsim", "Multiple reconstruction fraction",  "MultipleReco",    0.0, 0.01},
    {"hsec", "hrec", "Non-primary reconstruction fraction","Secondary",      0.0, 0.1 },
  };

  // Sanity check for samples sharing the same GEN-SIM(-DIGI): the simulated-particle
  // denominator (hsim) must then be identical. If it is not, the two jobs processed
  // different events (e.g. failed CRAB jobs) or the TrackingParticles differ.
  {
    bool saveCent = useCentrality;
    useCentrality = false; // all centralities: reconstructed centrality itself may differ between releases
    TH2* s1 = getHist2D(f1, "hsim", "check1");
    TH2* s2 = getHist2D(f2, "hsim", "check2");
    useCentrality = saveCent;
    TH1* v1 = (TH1*) f1->Get(Form("%s/vtxZ", dirName));
    TH1* v2 = (TH1*) f2->Get(Form("%s/vtxZ", dirName));
    if (s1 && s2) {
      double i1 = s1->Integral(), i2 = s2->Integral();
      int nDiffBins = 0;
      for (int bx = 0; bx <= s1->GetNbinsX()+1; bx++)
        for (int by = 0; by <= s1->GetNbinsY()+1; by++)
          if (s1->GetBinContent(bx, by) != s2->GetBinContent(bx, by)) nDiffBins++;
      std::cout << "Events (vtxZ entries): " << (v1 ? v1->GetEntries() : -1) << " vs " << (v2 ? v2->GetEntries() : -1) << std::endl;
      std::cout << "hsim integral:         " << i1 << " vs " << i2 << std::endl;
      if (nDiffBins == 0)
        std::cout << "hsim identical in both samples: same events and same TrackingParticles." << std::endl;
      else
        std::cout << "WARNING: hsim differs in " << nDiffBins << " (eta,pT) bins. If the samples share GEN-SIM-DIGI,"
                  << " the jobs did not process the same events (check failed CRAB jobs) or the TrackingParticles differ." << std::endl;
      delete s1; delete s2;
    }
  }

  TFile* fout = new TFile(Form("%s/compareTwoSamples.root", outDir), "RECREATE");

  for (auto& rt : rates) {
    TH2* n1 = getHist2D(f1, rt.num, "s1"); TH2* d1 = getHist2D(f1, rt.den, "s1");
    TH2* n2 = getHist2D(f2, rt.num, "s2"); TH2* d2 = getHist2D(f2, rt.den, "s2");
    if (!n1 || !d1 || !n2 || !d2) return;

    // vs pT, two eta ranges
    drawComparison(makeRate(n1, d1, true, 0, etaNarrow, rt.fig + "_pt_etaNarrow_s1"),
                   makeRate(n2, d2, true, 0, etaNarrow, rt.fig + "_pt_etaNarrow_s2"),
                   rt.title, rt.ymin, rt.ymax, true, label1, label2,
                   Form("|#eta| < %.1f (bin edges)", etaNarrow), rt.fig + "_vsPt_etaNarrow", fout);
    drawComparison(makeRate(n1, d1, true, 0, etaWide, rt.fig + "_pt_etaWide_s1"),
                   makeRate(n2, d2, true, 0, etaWide, rt.fig + "_pt_etaWide_s2"),
                   rt.title, rt.ymin, rt.ymax, true, label1, label2,
                   Form("|#eta| < %.1f", etaWide), rt.fig + "_vsPt_etaWide", fout);
    // vs eta, two pT thresholds
    drawComparison(makeRate(n1, d1, false, ptLow, 0, rt.fig + "_eta_ptLow_s1"),
                   makeRate(n2, d2, false, ptLow, 0, rt.fig + "_eta_ptLow_s2"),
                   rt.title, rt.ymin, rt.ymax, false, label1, label2,
                   Form("p_{T} > %.1f GeV", ptLow), rt.fig + "_vsEta_ptLow", fout);
    drawComparison(makeRate(n1, d1, false, ptHigh, 0, rt.fig + "_eta_ptHigh_s1"),
                   makeRate(n2, d2, false, ptHigh, 0, rt.fig + "_eta_ptHigh_s2"),
                   rt.title, rt.ymin, rt.ymax, false, label1, label2,
                   Form("p_{T} > %.1f GeV", ptHigh), rt.fig + "_vsEta_ptHigh", fout);
  }

  fout->Close();
  std::cout << "Plots saved in " << outDir << "/" << std::endl;
}

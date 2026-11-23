///Event-by-event comparison of the centrality bin (hiBin, 0-199 = 0-100% in 0.5% steps) between two
///reconstructions of the same events, from the anaTrack tree.
///
///Run:
///  root -l -b -q 'checkCentralityMigration.C("trk_rereco.root","trk_prompt.root","17_0_0","13_2_16_patch1","outdir")'
///Output: outdir/CentralityMigration.pdf (2D hiBin vs hiBin, and the per-event shift) and a printout.

#include "TFile.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TSystem.h"
#include <iostream>
#include <map>
#include <array>
#include <cmath>

namespace centmig {

typedef std::array<unsigned long long,3> Key;

bool readCent(const TString& fn, std::map<Key,int>& m)
{
  TFile* f = TFile::Open(fn);
  if (!f || f->IsZombie() || !f->Get("anaTrack/trackTree")) { std::cerr << "ERROR: anaTrack/trackTree not found in " << fn << std::endl; return false; }
  TTreeReader r("anaTrack/trackTree", f);
  TTreeReaderValue<int> run(r, "nRun"), lumi(r, "nLumi"), evt(r, "nEv"), cent(r, "centrality");
  while (r.Next()) m[{(unsigned long long)(unsigned)*run, (unsigned long long)(unsigned)*lumi, (unsigned long long)(unsigned)*evt}] = *cent;
  f->Close();
  return true;
}

void run(TString f1, TString f2, TString l1, TString l2, TString outDir)
{
  gStyle->SetOptStat(0);
  gSystem->Exec("mkdir -p " + outDir);
  std::map<Key,int> c1, c2;
  if (!readCent(f1, c1) || !readCent(f2, c2)) return;

  TH2D* h2 = new TH2D("hCent2D", Form(";hiBin %s;hiBin %s", l2.Data(), l1.Data()), 200, 0, 200, 200, 0, 200);
  TH1D* hd = new TH1D("hCentDiff", Form(";hiBin(%s) - hiBin(%s);Events", l1.Data(), l2.Data()), 81, -40.5, 40.5);
  long n = 0, same = 0, within2 = 0, within10 = 0, sameClass10 = 0;
  double sum = 0, sum2 = 0;
  for (auto& e : c1) {
    auto it = c2.find(e.first);
    if (it == c2.end()) continue;
    int a = e.second, b = it->second, d = a - b;
    n++; h2->Fill(b, a); hd->Fill(d);
    same += (d == 0); within2 += (std::abs(d) <= 2); within10 += (std::abs(d) <= 10);
    sameClass10 += (a / 20 == b / 20); // same 10%-wide class
    sum += d; sum2 += d * d;
  }
  if (n == 0) { std::cerr << "ERROR: no common events" << std::endl; return; }
  double mean = sum / n, rms = std::sqrt(sum2 / n - mean * mean);
  printf("Common events: %ld\n", n);
  printf("hiBin identical: %.1f%%, |shift| <= 1%% centrality (2 hiBins): %.1f%%, |shift| <= 5%% (10 hiBins): %.1f%%\n",
         100. * same / n, 100. * within2 / n, 100. * within10 / n);
  printf("Same 10%%-wide centrality class: %.1f%%\n", 100. * sameClass10 / n);
  printf("Shift hiBin(%s) - hiBin(%s): mean %.2f, rms %.2f hiBins (1 hiBin = 0.5%%)\n", l1.Data(), l2.Data(), mean, rms);

  TCanvas* c = new TCanvas("cCent", "", 1400, 650);
  c->Divide(2, 1);
  c->cd(1); gPad->SetRightMargin(0.13); gPad->SetLogz(); h2->Draw("colz");
  c->cd(2); gPad->SetLogy(); hd->SetLineWidth(2); hd->Draw("hist");
  c->Print(outDir + "/CentralityMigration.pdf");
  std::cout << "Saved " << outDir << "/CentralityMigration.pdf" << std::endl;
}

} // namespace centmig

void checkCentralityMigration(TString f1 = "trk1.root", TString f2 = "trk2.root",
                              TString l1 = "Sample 1", TString l2 = "Sample 2", TString outDir = "CentralityCheck")
{
  centmig::run(f1, f2, l1, l2, outDir);
}

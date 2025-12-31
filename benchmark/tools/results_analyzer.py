#!/usr/bin/env python3
"""
Analyze and compare benchmark results from the order book matching engine.
"""

import sys
import re
from dataclasses import dataclass
from typing import List, Dict
import json

@dataclass
class BenchmarkResult:
    name: str
    total_ops: int
    throughput: float
    avg_latency_ns: float
    p50_ns: float
    p95_ns: float
    p99_ns: float

def parse_results(filename: str) -> List[BenchmarkResult]:
    """Parse benchmark output file."""
    results = []
    
    with open(filename, 'r') as f:
        content = f.read()
    
    # Split by benchmark sections
    sections = re.split(r'===\s+(.+?)\s+===', content)
    
    i = 1
    while i < len(sections):
        name = sections[i].strip()
        data = sections[i + 1]
        
        # Extract metrics
        total_ops = re.search(r'Total operations:\s+(\d+)', data)
        throughput = re.search(r'Throughput:\s+([\d.]+)', data)
        avg_lat = re.search(r'Average latency:\s+([\d.]+)', data)
        p50 = re.search(r'P50 latency:\s+([\d.]+)', data)
        p95 = re.search(r'P95 latency:\s+([\d.]+)', data)
        p99 = re.search(r'P99 latency:\s+([\d.]+)', data)
        
        if all([total_ops, throughput, avg_lat, p50, p95, p99]):
            results.append(BenchmarkResult(
                name=name,
                total_ops=int(total_ops.group(1)),
                throughput=float(throughput.group(1)),
                avg_latency_ns=float(avg_lat.group(1)),
                p50_ns=float(p50.group(1)),
                p95_ns=float(p95.group(1)),
                p99_ns=float(p99.group(1))
            ))
        
        i += 2
    
    return results

def print_summary(results: List[BenchmarkResult]):
    """Print formatted summary of results."""
    print("\n" + "="*80)
    print("BENCHMARK SUMMARY")
    print("="*80)
    
    for r in results:
        print(f"\n{r.name}")
        print("-" * len(r.name))
        print(f"  Operations:       {r.total_ops:,}")
        print(f"  Throughput:       {r.throughput:,.0f} ops/sec")
        print(f"  Avg Latency:      {r.avg_latency_ns:.0f} ns ({r.avg_latency_ns/1000:.2f} us)")
        print(f"  P50:              {r.p50_ns:.0f} ns")
        print(f"  P95:              {r.p95_ns:.0f} ns ({r.p95_ns/1000:.2f} us)")
        print(f"  P99:              {r.p99_ns:.0f} ns ({r.p99_ns/1000:.2f} us)")

def compare_results(baseline_file: str, current_file: str):
    """Compare two benchmark runs and show differences."""
    baseline = {r.name: r for r in parse_results(baseline_file)}
    current = {r.name: r for r in parse_results(current_file)}
    
    print("\n" + "="*80)
    print("BENCHMARK COMPARISON")
    print("="*80)
    print(f"Baseline: {baseline_file}")
    print(f"Current:  {current_file}")
    
    for name in baseline.keys():
        if name not in current:
            print(f"\n{name}: NOT FOUND in current results")
            continue
        
        b = baseline[name]
        c = current[name]
        
        print(f"\n{name}")
        print("-" * len(name))
        
        # Throughput comparison
        throughput_diff = ((c.throughput - b.throughput) / b.throughput) * 100
        direction = "IMPROVED" if throughput_diff > 5 else "INCREASED" if throughput_diff > 0 else "DECREASED" if throughput_diff < -5 else "UNCHANGED"
        print(f"  Throughput:    {c.throughput:,.0f} ops/sec ({throughput_diff:+.1f}%) [{direction}]")
        
        # Latency comparison (lower is better)
        lat_diff = ((c.avg_latency_ns - b.avg_latency_ns) / b.avg_latency_ns) * 100
        direction = "IMPROVED" if lat_diff < -5 else "DECREASED" if lat_diff < 0 else "INCREASED" if lat_diff > 5 else "UNCHANGED"
        print(f"  Avg Latency:   {c.avg_latency_ns:.0f} ns ({lat_diff:+.1f}%) [{direction}]")
        
        # P99 comparison
        p99_diff = ((c.p99_ns - b.p99_ns) / b.p99_ns) * 100
        direction = "IMPROVED" if p99_diff < -5 else "DECREASED" if p99_diff < 0 else "INCREASED" if p99_diff > 5 else "UNCHANGED"
        print(f"  P99 Latency:   {c.p99_ns:.0f} ns ({p99_diff:+.1f}%) [{direction}]")
    
    # Overall assessment
    print("\n" + "="*80)
    avg_throughput_change = sum(
        ((current[n].throughput - baseline[n].throughput) / baseline[n].throughput) * 100
        for n in baseline.keys() if n in current
    ) / len(baseline)
    
    if avg_throughput_change > 5:
        print("RESULT: SIGNIFICANT IMPROVEMENT detected")
    elif avg_throughput_change < -5:
        print("RESULT: PERFORMANCE REGRESSION detected")
    else:
        print("RESULT: No significant change")
    
    print(f"Average throughput change: {avg_throughput_change:+.1f}%")

def export_json(results: List[BenchmarkResult], output_file: str):
    """Export results to JSON format."""
    data = [
        {
            "name": r.name,
            "total_ops": r.total_ops,
            "throughput_ops_per_sec": r.throughput,
            "avg_latency_ns": r.avg_latency_ns,
            "p50_latency_ns": r.p50_ns,
            "p95_latency_ns": r.p95_ns,
            "p99_latency_ns": r.p99_ns
        }
        for r in results
    ]
    
    with open(output_file, 'w') as f:
        json.dump(data, f, indent=2)
    
    print(f"\nResults exported to {output_file}")

def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 results_analyzer.py <results_file>           # Analyze single run")
        print("  python3 results_analyzer.py <baseline> <current>     # Compare two runs")
        print("  python3 results_analyzer.py <results_file> --json    # Export to JSON")
        sys.exit(1)
    
    if len(sys.argv) == 3 and sys.argv[2] == '--json':
        results = parse_results(sys.argv[1])
        print_summary(results)
        export_json(results, sys.argv[1].replace('.txt', '.json'))
    elif len(sys.argv) == 3:
        compare_results(sys.argv[1], sys.argv[2])
    else:
        results = parse_results(sys.argv[1])
        print_summary(results)

if __name__ == '__main__':
    main()

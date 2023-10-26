# AuRORA: Virtualized Accelerator Orchestration for Multi-Tenant Workloads

| This is an archival of the AuRORA (MICRO 2023) Chipyard artifact. |
|-----|

## Quick Links

* **MICRO2023 [paper](https://people.eecs.berkeley.edu/~ysshao/assets/papers/aurora-micro2023.pdf)**
* **Top level FireSim: [repo](https://github.com/SeahK/firesim-aurora-ae/tree/main), [zenodo](https://zenodo.org/records/8354298)**
* **Submodules (Zenodo): [chipyard](https://zenodo.org/records/8354250), [accelerator HW](https://zenodo.org/records/8354236) [SW](https://zenodo.org/records/8354218)**

This repository corresponds to submodule "chipyard" as ReRoCC AE version implements here


## What is AuRORA

AuRORA is a novel full-stack accelerator integration methodology that enables scalable multi-accelerator deployment for multi-tenant workloads.
AuRORA supports virtualized accelerator orchestration through co-designing the hw-sw stack of accelerator allow adaptively binding the workloads into  accelerators.
AuRORA consists of [ReRoCC](https://github.com/ucb-bar/rerocc) (remote RoCC), a virtualized and disaggregated accelerator interface for many-accelerator integration, and a runtime system for adaptive accelerator management.
Similar to virtual memory to physical memory abstraction, AuRORA provides an abstraction between user's view of accelerator and the physical accelerator instances.
AuRORA's virtualized interface allows workloads to be flexibly and dynamically orchestrated to available accelerators based on their latency requirement, regardless of physical accelerator instances' location.
To effectively support virtualized accelerator orchestration, AuRORA delivers a full-stack solution that co-designs the HW and SW layers, with the goal of delivering scalable performance for multiple accelerator system. 

![AuRORA full-stack accelerator integration methodology](./img/aurora-stack.png)

## Other Useful Links

### Using Chipyard
To learn about using Chipyard, see the documentation on the Chipyard documentation site: https://chipyard.readthedocs.io/

### Using FireSim
To learn about using FireSim, you can find the documentation and getting-started guide at
[docs.fires.im](https://docs.fires.im). 

### Using Gemmini
To learn about using Gemmini, visit Gemmini [repository](https://github.com/ucb-bar/gemmini/tree/dev)






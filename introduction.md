# Introduction

This project was developed as part of the **Distributed Embedded Systems (DES)** class. 

## Project Context and Objectives

The primary goal of this project is to build a Distributed Real-Time System (DRS) application capable of establishing a highly precise global time base across a dynamic cluster of hardware nodes (such as Raspberry Pis). The nodes communicate over mixed networks, including both Ethernet and Wi-Fi, utilizing UDP sockets in C/C++.

The overarching objective is to achieve a synchronization precision of **< 100 µs** between the nodes. Crucially, the synchronization protocol is designed to run entirely in the user-space of a standard Linux OS without the need for custom kernel drivers.

## Core Principles

The system architecture is guided by several core engineering requirements:

- **KISS Principle (Keep It Simple, Stupid)**: The system requires zero manual configuration regarding network addresses or node roles. When a new node is plugged in, it integrates seamlessly.
- **Autonomous Discovery**: Nodes discover each other dynamically without a centralized registry.
- **Resilience**: The protocol dynamically filters latency jitter (especially asymmetrical jitter introduced by Wi-Fi) and is resilient to crash failures, ensuring there is no single point of failure if a leader node crashes.
- **Verifiability**: The claimed synchronization precision must be objectively measurable and falsifiable using external verification tools to prove physical simultaneity, rather than relying solely on internal software timestamps.

## Authorship and Report Structure

It is important to note that both the codebase for this project and the generation of this report were heavily assisted by Artificial Intelligence (AI). The underlying system architecture and engineering requirements, however, were carefully thought out and designed by the architecture team.

The remainder of this report is structured to walk through these architectural decisions in detail. The subsequent chapters outline the complete system architecture—ranging from the hardware environment and network topology to the specific wire protocol and synchronization mathematics. Finally, the report concludes with an overview of the system's telemetry data formatting and the development workflow used to build and verify the cluster.

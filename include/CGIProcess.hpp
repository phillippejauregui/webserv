/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIProcess.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/01 12:00:00 by lylrandr          #+#    #+#             */
/*   Updated: 2026/06/01 16:33:44 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIPROCESS_HPP
#define CGIPROCESS_HPP

#include <string>
#include <ctime>
#include <sys/types.h>

struct CGIProcess {
    pid_t       pid;
    int         inFd;
    int         outFd;
    std::string inputBuffer;
    size_t      inputOffset;
    std::string outputBuffer;
    bool        inputDone;
    bool        outputDone;
    time_t      startTime;

    CGIProcess()
        : pid(-1), inFd(-1), outFd(-1),
          inputOffset(0), inputDone(false), outputDone(false),
          startTime(0) {}
};

#endif
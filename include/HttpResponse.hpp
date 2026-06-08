/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/13 15:57:48 by lylrandr          #+#    #+#             */
/*   Updated: 2026/06/01 16:21:37 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

# include <string>
# include <map>
# include <dirent.h>

struct	HttpResponse{
	int									statusCode;
	std::string							statusMessage;
	std::map<std::string, std::string>	headers;
	std::string							body;
	bool								isCGIPending;
};

#endif

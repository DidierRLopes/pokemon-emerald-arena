import type { Metadata } from "next";
import { headers } from "next/headers";
import "./globals.css";

export async function generateMetadata(): Promise<Metadata> {
  const request = await headers();
  const host = request.get("x-forwarded-host") || request.get("host") || "localhost:3000";
  const protocol = host.startsWith("localhost") || host.startsWith("127.0.0.1") ? "http" : "https";
  const origin = new URL(`${protocol}://${host}`);
  const title = "Emerald Arena — Real-time Pokémon Emerald";
  const description = "The original Pokémon Emerald, modified for real-time battles.";
  const image = new URL("/og.png", origin).href;
  return {
    title, description, metadataBase: origin,
    openGraph: { title, description, type: "website", url: new URL("/arena", origin).href,
      images: [{ url: image, alt: "Emerald Arena. Pokémon Emerald, modified for real-time battles." }] },
    twitter: { card: "summary_large_image", title, description, images: [image] },
  };
}

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <head>
        <link rel="preload" href="/fonts/tiny5-regular.ttf" as="font" type="font/ttf" crossOrigin="anonymous" />
      </head>
      <body>
        {children}
      </body>
    </html>
  );
}
